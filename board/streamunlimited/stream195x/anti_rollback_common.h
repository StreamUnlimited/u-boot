#ifndef __ANTI_ROLLBACK_COMMON_H__
#define __ANTI_ROLLBACK_COMMON_H__

#include <asm/types.h>

/**
 * Performs anti-rollback checks on locked boards, verifying image's
 * security version against what is stored in fuses.
 *
 * @param image_name Name of the image being evaluated. Only used in printf.
 * @param image_security_version The image's security version.
 * @return 0 if anti-rollback check passed, 1 if check failed.
 */
int perform_anti_rollback_check(const char *image_name, u32 image_security_version);

#endif /* __ANTI_ROLLBACK_COMMON_H__ */
