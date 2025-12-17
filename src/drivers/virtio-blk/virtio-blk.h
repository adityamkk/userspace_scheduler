#pragma once

struct virtio_virtq *virtq_init(unsigned index);
extern void virtio_blk_init(void);
extern void read_write_disk(void *buf, unsigned sector, int is_write);