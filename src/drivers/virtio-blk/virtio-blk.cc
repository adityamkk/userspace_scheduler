#include "virtio-blk.h"
#include "virtio.h"
#include "../../heap.h"
#include "../../sync/pool.h"
#include "../../sync/syncmap.h"
#include "../../pallocator.h"

// https://operating-system-in-1000-lines.vercel.app/en/15-virtio-blk

struct virtio_virtq *blk_request_vq; // Only 1 virtq, so this is global
uint64_t blk_capacity; // Only 1 virtq, so this is global
Pool<int>* descriptor_pool; // Pool of descriptors per virtq

struct BlockRequest {
    SharedPtr<Promise<bool>> blk_promise;
    void* buf;
    uint32_t sector;
    int is_write;
    int desc_id;
    int data_id;
    int status_id;
    struct virtio_blk_req * blk_req;

    BlockRequest(void* buf, uint32_t sector, int is_write, int desc_id, int data_id, int status_id, paddr_t blk_req) {
        blk_promise = SharedPtr<Promise<bool>>(new Promise<bool>());
        this->buf = buf;
        this->sector = sector;
        this->is_write = is_write;
        this->desc_id = desc_id;
        this->data_id = data_id;
        this->status_id = status_id;
        this->blk_req = (struct virtio_blk_req *)blk_req;
    }
};

SyncMap<int,SharedPtr<BlockRequest>>* req_promises;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
struct virtio_virtq *virtq_init(unsigned index) {
    // Allocate a region for the virtqueue.
    paddr_t virtq_paddr = (paddr_t)(new virtio_virtq());
    struct virtio_virtq *vq = (struct virtio_virtq *) virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *) &vq->used.index;

    ASSERT(vq->avail.flags == 0);
    // Ideally descriptor_pool is per-virtq instead of global
    descriptor_pool = new Pool<int>();
    for (int i = 0; i < VIRTQ_ENTRY_NUM; i++) {
        descriptor_pool->free(new int(i)); // Mark this as an available descriptor
    }
    req_promises = new SyncMap<int,SharedPtr<BlockRequest>>(VIRTQ_ENTRY_NUM);
    // 1. Select the queue writing its index (first queue is 0) to QueueSel.
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    // 5. Notify the device about the queue size by writing the size to QueueNum.
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    // 6. Notify the device about the used alignment by writing its value in bytes to QueueAlign.
    virtio_reg_write32(VIRTIO_REG_QUEUE_ALIGN, 0);
    // 7. Write the physical number of the first page of the queue to the QueuePFN register.
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr);
    return vq;
}
#pragma GCC diagnostic pop

void virtio_blk_init(void) {
    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
        PANIC("| virtio: invalid magic value");
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
        PANIC("| virtio: invalid version");
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        PANIC("| virtio: invalid device id");

    // 1. Reset the device.
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
    // 2. Set the ACKNOWLEDGE status bit: the guest OS has noticed the device.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    // 3. Set the DRIVER status bit.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
    // 5. Set the FEATURES_OK status bit.
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_FEAT_OK);
    // 7. Perform device-specific setup, including discovery of virtqueues for the device
    blk_request_vq = virtq_init(0);
    // 8. Set the DRIVER_OK status bit.
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

    // Get the disk capacity.
    blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
    printf("| virtio-blk: capacity is %d bytes\n", (int)blk_capacity);
}

// Notifies the device that there is a new request. `desc_index` is the index
// of the head descriptor of the new request.
SpinlockNoInterrupts kickLock;
void virtq_kick(struct virtio_virtq *vq, int desc_index) {
    kickLock.lock();
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index); // Interrupts
    vq->last_used_index++; // Comment out line for Interrupts, uncomment for Polling
    kickLock.unlock();
}

// Returns whether there are requests being processed by the device.
bool virtq_is_busy(struct virtio_virtq *vq) {
    return vq->last_used_index != *vq->used_index;
}

// Reads/writes from/to virtio-blk device.
SharedPtr<Promise<bool>> read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector=%d, but capacity is %d\n",
              sector, blk_capacity / SECTOR_SIZE);
        SharedPtr<Promise<bool>> failure_promise = SharedPtr<Promise<bool>>(new Promise<bool>());
        failure_promise->set(false);
        return failure_promise;
    }

    // Collect 3 descriptors from the pool of descriptors
    int* desc_id_ptr = descriptor_pool->allocate();
    int* data_id_ptr = descriptor_pool->allocate();
    int* status_id_ptr = descriptor_pool->allocate();
    if (desc_id_ptr == nullptr || data_id_ptr == nullptr || status_id_ptr == nullptr) {
        descriptor_pool->free(desc_id_ptr);
        descriptor_pool->free(data_id_ptr);
        descriptor_pool->free(status_id_ptr);
        printf("virtio: warn: failed to allocate descriptors in virtq\n");
        SharedPtr<Promise<bool>> failure_promise = SharedPtr<Promise<bool>>(new Promise<bool>());
        failure_promise->set(false);
        return failure_promise;
    }
    int desc_id = *desc_id_ptr; // TODO: Delete the pointers here
    int data_id = *data_id_ptr;
    int status_id = *status_id_ptr;

    // Dynamically allocate a block request
    paddr_t blk_req_paddr = (paddr_t)(new virtio_blk_req()); //pallocator::alloc_page();
    struct virtio_blk_req * blk_req = (struct virtio_blk_req *) blk_req_paddr;

    req_promises->put(desc_id, SharedPtr<BlockRequest>(new BlockRequest(buf, sector, is_write, desc_id, data_id, status_id, blk_req_paddr)));

    // Construct the request according to the virtio-blk specification.
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    // Construct the virtqueue descriptors (using 3 descriptors).
    struct virtio_virtq *vq = blk_request_vq;
    vq->descs[desc_id].addr = blk_req_paddr;
    vq->descs[desc_id].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
    vq->descs[desc_id].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[desc_id].next = data_id;

    vq->descs[data_id].addr = blk_req_paddr + offsetof(struct virtio_blk_req, data);
    vq->descs[data_id].len = SECTOR_SIZE;
    vq->descs[data_id].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[data_id].next = status_id;

    vq->descs[status_id].addr = blk_req_paddr + offsetof(struct virtio_blk_req, status);
    vq->descs[status_id].len = sizeof(uint8_t);
    vq->descs[status_id].flags = VIRTQ_DESC_F_WRITE;

    // Notify the device that there is a new request.
    //printf("Kick\n");
    virtq_kick(vq, desc_id);

    // Wait until the device finishes processing.
    while (virtq_is_busy(vq))
        ;

    // virtio-blk: If a non-zero value is returned, it's an error.
    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d status=%d\n",
               sector, blk_req->status);
        delete blk_req;

        SharedPtr<Promise<bool>> failure_promise = req_promises->get(desc_id)->blk_promise;
        failure_promise->set(false);
        req_promises->remove(desc_id);
        descriptor_pool->free(desc_id_ptr);
        descriptor_pool->free(data_id_ptr);
        descriptor_pool->free(status_id_ptr);
        return failure_promise;
    }

    // For read operations, copy the data into the buffer.
    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);

    delete blk_req;
    SharedPtr<Promise<bool>> success_promise = req_promises->get(desc_id)->blk_promise;
    success_promise->set(true);
    req_promises->remove(desc_id);
    descriptor_pool->free(desc_id_ptr);
    descriptor_pool->free(data_id_ptr);
    descriptor_pool->free(status_id_ptr);
    return success_promise;
}

// TODO: Need to make this O(1)
void virtio_blk_isr() {
    struct virtio_virtq *vq = blk_request_vq;
    //printf("ISR Gotten!!!\n");
    while (vq->last_used_index < *vq->used_index) {
        // Process the used entry
        int desc_id = (int)vq->used.ring[vq->last_used_index % VIRTQ_ENTRY_NUM].id;
        //printf("ISR Processing used entry for desc_id = %d\n", desc_id);
        SharedPtr<BlockRequest> request = req_promises->get(desc_id);
        SharedPtr<Promise<bool>> promise = request->blk_promise;
        ASSERT(promise != nullptr);
        if (request->blk_req->status != 0) {
            printf("virtio: warn: failed to read/write sector=%d status=%d\n",
                request->sector, request->blk_req->status);
            delete request->blk_req;

            // req_promises->remove(desc_id); // Why remove the request? Don't I still need it in the readwrite?
            descriptor_pool->free(new int(request->desc_id));
            descriptor_pool->free(new int(request->data_id));
            descriptor_pool->free(new int(request->status_id));
            promise->set(false);
            vq->last_used_index++;
            return;
        }
        // req_promises->remove(desc_id); // // Why remove the request? Don't I still need it in the readwrite?
        descriptor_pool->free(new int(request->desc_id));
        descriptor_pool->free(new int(request->data_id));
        descriptor_pool->free(new int(request->status_id));
        if (!request->is_write) {
            memcpy(request->buf, request->blk_req->data, SECTOR_SIZE);
        }
        //printf("Setting promise to true for desc_id = %d\n", desc_id);
        promise->set(true);
        vq->last_used_index++;
    }
}