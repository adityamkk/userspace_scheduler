#pragma once

#include "../../sync/promise.h"
#include "../../sync/shared.h"

struct virtio_virtq *virtq_init(unsigned index);
extern void virtio_blk_init(void);
extern SharedPtr<Promise<bool>> read_write_disk(void *buf, unsigned sector, int is_write);