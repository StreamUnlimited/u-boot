#ifndef __SPL_ANTI_ROLLBACK_H__
#define __SPL_ANTI_ROLLBACK_H__

/**
 * Performs anti-rollback checks on locked boards, verifying SPL's version
 * against what is stored in fuses.
 */
void spl_anti_rollback_check(void);

/**
 * Verifies security version of the FIT image that is loaded by SPL against
 * what is stored in fuses.
 *
 * Note: this function is prefixed 'sue', because it's called from iMX's SPL code,
 * to make this "customization" easy to spot.
 *
 * @param fit Pointer to the FIT image.
 */
void sue_spl_fit_anti_rollback_check(void *fit);

#endif /* __SPL_ANTI_ROLLBACK_H__ */
