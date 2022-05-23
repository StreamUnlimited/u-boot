#include "spl_anti_rollback.h"
#include "anti_rollback_common.h"

#include <asm/io.h>
#include <asm/mach-imx/hab.h>
#include <asm/sections.h>
#include <common.h>

/*
 * Offset of the current image's security version, relative to the end of U-boot SPL.
 * NOTE: The offset HAS TO MATCH firmwares appended in function assemble_u-boot-spl-ddr-secver
 *       in StreamBSP/meta-soc/recipes-imx8/imx8-secureboot/files/sign_swupdate.sh
 */
#if defined(CONFIG_TARGET_STREAM195X_EMMC)
/* Security version is appended after the DDR4 timing files (imem_1d, dmem_1d, imem_2d, dmem_2d) */
#define IMAGE_SECURITY_VERSION_OFFSET	(0x8000 + 0x4000 + 0x8000 + 0x0800)
#elif defined(CONFIG_NAND_MXS)
/* Security version is appended after the DDR3 timing files (imem_1d, dmem_1d) */
#define IMAGE_SECURITY_VERSION_OFFSET	(0x8000 + 0x4000)
#else
#error "Unknown Stream195X configuration"
#endif

/* Anti rollback structure */
#define ANTI_ROLLBACK_MAGIC 0xc0ffee
typedef struct __attribute__((__packed__))
{
	/* We expect ANTI_ROLLBACK_MAGIC */
	u32 magic;

	/* Security version itself */
	u32 image_security_version;
}
anti_rollback_version;

/*
 * Verifies SPL's image security version against what is stored in fuses.
 */
void spl_anti_rollback_check(void)
{
#ifdef CONFIG_SECURE_BOOT
	anti_rollback_version *arbv;
	u32 arb_magic;
	u32 arb_image_security_version;
	int ret;

	if (imx_hab_is_enabled()) {
		arbv = (anti_rollback_version *) ((ulong) &_end + IMAGE_SECURITY_VERSION_OFFSET);

		/* All fields are stored in big endian, so let's convert them */
		arb_magic = be32_to_cpu(arbv->magic);
		arb_image_security_version = be32_to_cpu(arbv->image_security_version);

		if (arb_magic != ANTI_ROLLBACK_MAGIC) {
			printf("ERROR: contents of anti-rollback's magic field differ: "
				"expected 0x%08x, got 0x%08x\n", ANTI_ROLLBACK_MAGIC, arb_magic);
			hang();
		}

		ret = perform_anti_rollback_check("SPL", arb_image_security_version);
		if (ret) {
			/* Halt if there was some error */
			hang();
		}
	}
#endif
}
