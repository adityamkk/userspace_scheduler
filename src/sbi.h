#pragma once

struct sbiret {
    long error;
    long value;
};

extern struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4, long arg5, long fid, long eid);

extern void putchar(char ch);