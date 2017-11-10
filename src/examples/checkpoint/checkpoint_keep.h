#ifndef __CHECKPOINT_KEEP_H__
#define __CHECKPOINT_KEEP_H__

#ifdef __cplusplus
extern "C"{
#endif
extern struct timespec *ckp_checkpoint_timestamp;
extern struct timespec *ckp_rollback_timestamp;
#ifdef __cplusplus
}
#endif

#endif
