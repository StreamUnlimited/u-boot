#include "anti_rollback_common.h"

#include <common.h>
#include <fuse.h>

/* We use General Purpose fuse register 1, which has 64 bits (i. e. 2 words) */
#define GENERAL_PURPOSE1_OTP_BANK		14
#define GENERAL_PURPOSE1_OTP_LOW_WORD	0
#define GENERAL_PURPOSE1_OTP_HIGH_WORD	1

/* The maximum allowed security number */
#define MAX_IMAGE_SECURITY_VERSION		63

/*
 * Helper function to create a version number out of the fuse bitmask.
 * This essentially just counts the number of set bits.
 * For example, fuse bitmask of 0x1f corresponds to a version number of 5.
 */
static u32 create_version_number_from_fuse_bitmask(u64 bitmask)
{
	u32 count = 0;

	while (bitmask) {
		count += bitmask & 1;
		bitmask >>= 1;
	}

	return count;
}

/*
 * Helper function to create a fuse bitmask out of the version number.
 * For example, version number 5 corresponds to 5 lowest bits set to 1, i. e. 0x1f
 */
static u64 create_fuse_bitmask_from_version_number(u32 version)
{
	u64 bitmask = 0xffffffffffffffff;
	bitmask <<= version;
	bitmask = ~bitmask;
	return bitmask;
}

/* Function to actually perform the anti-rollback check & fuse update */
int perform_anti_rollback_check(const char *image_name, u32 image_security_version)
{
	u32 value;
	u64 current_fuse_bitmask = 0;
	u32 device_security_version = 0;
	int ret;

	printf("\nPerforming anti-rollback check on image '%s'\n", image_name);

	ret = fuse_read(GENERAL_PURPOSE1_OTP_BANK, GENERAL_PURPOSE1_OTP_LOW_WORD, &value);
	if (ret) {
		printf("ERROR: Unable to read GP1 fuse register low word\n");
		return 1;
	}

	/* Initialize lower half of the bitmask value */
	current_fuse_bitmask = value;

	ret = fuse_read(GENERAL_PURPOSE1_OTP_BANK, GENERAL_PURPOSE1_OTP_HIGH_WORD, &value);
	if (ret) {
		printf("ERROR: Unable to read GP1 fuse register high word\n");
		return 1;
	}

	/* Initialize upper half of the bitmask value */
	current_fuse_bitmask |= ((u64) value) << 32;
	device_security_version = create_version_number_from_fuse_bitmask(current_fuse_bitmask);

	/* Sanity check */
	if (image_security_version > MAX_IMAGE_SECURITY_VERSION) {
		printf("WARN: Image security version is %u, which is greater than max (%u), capping\n",
			image_security_version, MAX_IMAGE_SECURITY_VERSION);
		image_security_version = MAX_IMAGE_SECURITY_VERSION;
	}

	/* Now perform the actual comparison. */
	if (device_security_version < image_security_version) {
		/*
		 * A new image was flashed with an increased security version number,
		 * we need to burn fuses to match it.
		 */
		u64 new_fuse_bitmask = create_fuse_bitmask_from_version_number(image_security_version);
		u32 new_fuse_bitmask_low = new_fuse_bitmask & 0xffffffff;
		u32 new_fuse_bitmask_high = (new_fuse_bitmask & 0xffffffff00000000) >> 32;

		printf("Need to program new device security version bitmask, low: 0x%8.8x, high: 0x%8.8x\n",
			new_fuse_bitmask_low, new_fuse_bitmask_high);

		ret = fuse_prog(GENERAL_PURPOSE1_OTP_BANK, GENERAL_PURPOSE1_OTP_LOW_WORD, new_fuse_bitmask_low);
		if (ret) {
			printf("ERROR: unable to program GP1 fuse register low word\n");
			return 1;
		}

		ret = fuse_prog(GENERAL_PURPOSE1_OTP_BANK, GENERAL_PURPOSE1_OTP_HIGH_WORD, new_fuse_bitmask_high);
		if (ret) {
			printf("ERROR: unable to program GP1 fuse register high word\n");
			return 1;
		}
	}
	else if (device_security_version > image_security_version) {
		/* Somehow an old image has made it on to the device */
		printf("ERROR: Anti-rollback check failed, device security version: %u, image security version: %u\n",
			device_security_version, image_security_version);

		return 1;
	}
	else {
		/*
		 * The security version stored in fuses matches the image security version.
		 * This state is the most common.
		 */
		assert(device_security_version == image_security_version);
	}

	printf("Anti-rollback check on image '%s' passed. Current image security version: %u\n\n",
		image_name, image_security_version);
	return 0;
}
