#include "virtio-blk.h"
#include "virtio.h"
#include "../../heap.h"
#include "../../sync/pool.h"

// https://operating-system-in-1000-lines.vercel.app/en/15-virtio-blk

struct virtio_virtq *blk_request_vq; // Only 1 virtq, so this is global
uint64_t blk_capacity; // Only 1 virtq, so this is global
Pool<int> descriptor_pool; // Pool of descriptors per virtq


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
struct virtio_virtq *virtq_init(unsigned index) {
    // Allocate a region for the virtqueue.
    paddr_t virtq_paddr = (paddr_t)(new virtio_virtq());
    struct virtio_virtq *vq = (struct virtio_virtq *) virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *) &vq->used.index;

    // Ideally descriptor_pool is per-virtq instead of global
    for (int i = 0; i < VIRTQ_ENTRY_NUM; i++) {
        descriptor_pool.free(new int(i)); // Mark this as an available descriptor
    }
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
void virtq_kick(struct virtio_virtq *vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

// Returns whether there are requests being processed by the device.
bool virtq_is_busy(struct virtio_virtq *vq) {
    return vq->last_used_index != *vq->used_index;
}

// Reads/writes from/to virtio-blk device.
void read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector=%d, but capacity is %d\n",
              sector, blk_capacity / SECTOR_SIZE);
        return;
    }

    // Dynamically allocate a block request
    paddr_t blk_req_paddr = (paddr_t)(new virtio_blk_req());
    struct virtio_blk_req * blk_req = (struct virtio_blk_req *) blk_req_paddr;

    // Construct the request according to the virtio-blk specification.
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    // Collect 3 descriptors from the pool of descriptors
    int* desc_id_ptr = descriptor_pool.allocate();
    int* data_id_ptr = descriptor_pool.allocate();
    int* status_id_ptr = descriptor_pool.allocate();
    if (desc_id_ptr == nullptr || data_id_ptr == nullptr || status_id_ptr == nullptr) {
        descriptor_pool.free(desc_id_ptr);
        descriptor_pool.free(data_id_ptr);
        descriptor_pool.free(status_id_ptr);
        printf("virtio: warn: failed to allocate descriptors in virtq\n");
        return;
    }
    int desc_id = *desc_id_ptr;
    int data_id = *data_id_ptr;
    int status_id = *status_id_ptr;

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
    virtq_kick(vq, desc_id);

    // Wait until the device finishes processing.
    while (virtq_is_busy(vq))
        ;

    // virtio-blk: If a non-zero value is returned, it's an error.
    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d status=%d\n",
               sector, blk_req->status);
        delete blk_req;
        return;
    }

    // For read operations, copy the data into the buffer.
    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);

    delete blk_req;
}