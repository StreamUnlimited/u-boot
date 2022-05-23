#ifndef __SPL_ANTI_ROLLBACK_H__
#define __SPL_ANTI_ROLLBACK_H__

/**
 * Performs anti-rollback checks on locked boards, verifying SPL's version
 * against what is stored in fuses.
 */
void spl_anti_rollback_check(void);

#endif /* __SPL_ANTI_ROLLBACK_H__ */
