// SPDX-License-Identifier: GPL-2.0+
/*
* Realtek AVB support
*
* Copyright (C) 2023, Realtek Corporation. All rights reserved.
*/

#include <stdio.h>
#include <string.h>
#include <env.h>
#include <fdtdec.h>
#include <command.h>
#include <common.h>
#include <console.h>
#include <malloc.h>
#include <mapmem.h>
#include <mtd.h>
#include <dm/devres.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <misc.h>
#include <dm/uclass.h>

#include <../lib/libavb/libavb.h>
#include <../lib/libavb/avb_sha.h>

#define	CERT_VBMETA_PK_HASH_OFFSET		0xd8
#define VBMETA_ROLLBACK_INDEX_OFFSET	0x70
#define ROLLBACK_INDEX_BYTES			8
#define EFUSE_ROLLBACK_INDEX_OFFSET		0x390
#define ROLLBACK_EFUSE_BYTES			32

struct mtd_avb_parameters {
	uint64_t image_size;
	const uint8_t* salt;
	uint32_t salt_len;
	const uint8_t* digest;
	uint32_t digest_len;
	const uint8_t* public_key;
	uint32_t public_key_len;
	char hash_algorithm[32];
	uint32_t rollback_index;
};

volatile int avb_flag = 0;

static void realtek_organize_cmdline(const uint8_t* kernel_cmdline, unsigned long dtb_blob)
{
	const char *common_bootargs = NULL;
	/* Do not revise the const chars in dtb and vbmeta buf. */
	char cmd_cp[CONFIG_SYS_CBSIZE];
	char kernel_cmd_cp[CONFIG_SYS_CBSIZE];
	/* Use dtb and vbmeta buf bootargs to combine as a new one. */
	char secure_bootargs[CONFIG_SYS_CBSIZE] = "setenv bootargs ";

	char dm_create[] = "dm-mod.create=\\\"system,,0,ro, 0 ";
	char block_dev[] = "/dev/ubiblock0_0";
	char other_options[] = "2 ignore_corruption ignore_zero_blocks\\\" root=/dev/dm-0 rootfstype=squashfs";
	const char m[] = " ";
	int index = 0;
	char *p = NULL;

	common_bootargs = fdtdec_get_chosen_prop((const void *)dtb_blob, "bootargs");
	strcpy(cmd_cp, common_bootargs);
	p = strtok(cmd_cp, m);
	while(p) {
		if (strncmp(p, "root", strlen("root")) != 0) {
			strcat(secure_bootargs, p);
			strcat(secure_bootargs, m);
		}
		p = strtok(NULL, m);
	}
	strcat(secure_bootargs, dm_create);

	strcpy(kernel_cmd_cp, (char *)kernel_cmdline);
	p = strtok((char *)kernel_cmd_cp, m);
	while(p) {
		switch (index) {
			case 5:
			case 6:
			case 7:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
			case 16:
				strcat(secure_bootargs, p);
				strcat(secure_bootargs, m);
				break;
			case 8:
			case 9:
				strcat(secure_bootargs, block_dev);
				strcat(secure_bootargs, m);
				break;
			default:
				break;
		}
		p = strtok(NULL, m);
		index++;
	}
	strcat(secure_bootargs, other_options);
    run_command(secure_bootargs, 0);
}

static int realtek_avb_sha(unsigned long image_buf,
	uint64_t image_size, char *hash_algorithm,
	const uint8_t* salt, uint32_t salt_len,
	const uint8_t* digest, uint32_t digest_len)
{
	AvbSHA256Ctx sha256_ctx;
	AvbSHA512Ctx sha512_ctx;
	const uint8_t* cal_digest = NULL;
	uint32_t cal_digest_len;

	if (avb_strcmp((const char*)hash_algorithm, "sha256") == 0) {
		avb_sha256_init(&sha256_ctx);
		avb_sha256_update(&sha256_ctx, salt, salt_len);
		avb_sha256_update(&sha256_ctx, (const u8 *)image_buf, image_size);
		cal_digest = avb_sha256_final(&sha256_ctx);
		cal_digest_len = AVB_SHA256_DIGEST_SIZE;
	} else if (avb_strcmp((const char*)hash_algorithm, "sha512") == 0) {
		avb_sha512_init(&sha512_ctx);
		avb_sha512_update(&sha512_ctx, salt, salt_len);
		avb_sha512_update(&sha512_ctx, (const u8 *)image_buf, image_size);
		cal_digest = avb_sha512_final(&sha512_ctx);
		cal_digest_len = AVB_SHA512_DIGEST_SIZE;
	} else {
		printf("Unsupported hash algorithm\n");
		return CMD_RET_FAILURE;
	}

	if (cal_digest_len != digest_len) {
		printf("Digest in descriptor is not of expected size\n");
		return CMD_RET_FAILURE;
	}

	if (avb_safe_memcmp(cal_digest, digest, digest_len) != 0) {
		printf("Hash of data does not match digest in descriptor\n");
		return  CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

int realtek_avb_support(
	unsigned long ddr_kernel_addr, unsigned long ddr_dtb_addr, const u8 *vbmeta_buf, u64 vbmeta_len)
{
	const AvbDescriptor** descriptors = NULL;
	size_t num_descriptors;
	int n, ret;
	const uint8_t* partition_name = NULL;
	struct mtd_avb_parameters kernel_param;
	struct mtd_avb_parameters dtb_param;
	char target_partition_kernel[] = "kernel";	// verify images whose name is started with kernel.
	char target_partition_dtb[] = "dtb"; 		// verify images whose name is started with dtb.
	const uint8_t* kernel_cmdline = NULL;

	/* Init params. */
	kernel_param.digest_len = 0;
	dtb_param.digest_len = 0;

	descriptors = avb_descriptor_get_all(vbmeta_buf, vbmeta_len, &num_descriptors);
	for (n = 0; n < num_descriptors; n++) {
		AvbDescriptor desc;

		if (!avb_descriptor_validate_and_byteswap(descriptors[n], &desc)) {
			printf("Error: Invalid descriptor\n");
			ret = CMD_RET_FAILURE;
			goto out;
		}

		switch (desc.tag) {
		case AVB_DESCRIPTOR_TAG_HASH: {
			AvbHashDescriptor hash_desc;

			if (!avb_hash_descriptor_validate_and_byteswap((const AvbHashDescriptor*)descriptors[n], &hash_desc)) {
				printf("Error: AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA\n");
				ret = CMD_RET_FAILURE;
				goto out;
			}

			partition_name = ((const uint8_t*)descriptors[n]) + sizeof(AvbHashDescriptor);
			if (memcmp(target_partition_kernel, partition_name, strlen(target_partition_kernel)) == 0) {
				kernel_param.image_size = hash_desc.image_size;
				memcpy(kernel_param.hash_algorithm, hash_desc.hash_algorithm, 32);

				kernel_param.salt = partition_name + hash_desc.partition_name_len;
				kernel_param.salt_len = hash_desc.salt_len;
				kernel_param.digest = kernel_param.salt + hash_desc.salt_len;
				kernel_param.digest_len = hash_desc.digest_len;
			} else 	if (memcmp(target_partition_dtb, partition_name, strlen(target_partition_dtb)) == 0) {
				dtb_param.image_size = hash_desc.image_size;
				memcpy(dtb_param.hash_algorithm, hash_desc.hash_algorithm, 32);

				dtb_param.salt = partition_name + hash_desc.partition_name_len;
				dtb_param.salt_len = hash_desc.salt_len;
				dtb_param.digest = dtb_param.salt + hash_desc.salt_len;
				dtb_param.digest_len = hash_desc.digest_len;
			}
		}
			break;

		case AVB_DESCRIPTOR_TAG_CHAIN_PARTITION: {
			AvbChainPartitionDescriptor chain_desc;

			if (!avb_chain_partition_descriptor_validate_and_byteswap(
					(AvbChainPartitionDescriptor*)descriptors[n], &chain_desc)) {
				printf("Error: Invalid chain partition descriptor\n");
				ret = CMD_RET_FAILURE;
				goto out;
			}

			partition_name = ((const uint8_t*)descriptors[n]) + sizeof(AvbChainPartitionDescriptor);
			if (memcmp(target_partition_kernel, partition_name, strlen(target_partition_kernel)) == 0) {
				if (chain_desc.rollback_index_location == 0) {
					printf("Error: Invalid chain partition, rollback index location error\n");
					ret = CMD_RET_FAILURE;
					goto out;
				}
				kernel_param.rollback_index = chain_desc.rollback_index_location;
				kernel_param.public_key = partition_name + chain_desc.partition_name_len;
				kernel_param.public_key_len = chain_desc.public_key_len;
			}
		}
			break;

		case AVB_DESCRIPTOR_TAG_KERNEL_CMDLINE: {
			AvbKernelCmdlineDescriptor kernel_cmdline_desc;

			if (!avb_kernel_cmdline_descriptor_validate_and_byteswap(
					(AvbKernelCmdlineDescriptor*)descriptors[n],
					&kernel_cmdline_desc)) {
				printf("Error: Invalid kernel cmdline descriptor\n");
				ret = CMD_RET_FAILURE;
				goto out;
			}

            if (!kernel_cmdline) {
			    kernel_cmdline = ((const uint8_t*)descriptors[n]) +	sizeof(AvbKernelCmdlineDescriptor);
            } else {
                continue;
            }
		}
			break;
		case AVB_DESCRIPTOR_TAG_HASHTREE:
		case AVB_DESCRIPTOR_TAG_PROPERTY:
			/* Do nothing. */
			break;
		}
	}

	if (!kernel_param.digest_len) {
		printf("Warning: kernel partition name not matched. Nothing to be verified");
		ret = CMD_RET_FAILURE;
		goto out;
	}
	if (realtek_avb_sha(ddr_kernel_addr, kernel_param.image_size, kernel_param.hash_algorithm, kernel_param.salt,
		kernel_param.salt_len, kernel_param.digest, kernel_param.digest_len) == CMD_RET_SUCCESS) {
        printf("Kernel Image verified success\n");
		ret = CMD_RET_SUCCESS;
	} else {
		printf("Error: kernel partition sha verify failed\n");
		ret = CMD_RET_FAILURE;
		goto out;
	}

	if (!dtb_param.digest_len) {
		printf("Warning: DTB partition name not matched, nothing to be verified\n");
		ret = CMD_RET_FAILURE;
		goto out;
	}
	if (realtek_avb_sha(ddr_dtb_addr, dtb_param.image_size, dtb_param.hash_algorithm, dtb_param.salt,
		dtb_param.salt_len, dtb_param.digest, dtb_param.digest_len) == CMD_RET_SUCCESS) {
        printf("DTB/FDT Image verified success!\n");
		ret = CMD_RET_SUCCESS;
	} else {
		printf("Error: DTB partition SHA verify failed\n");
		ret = CMD_RET_FAILURE;
		goto out;
	}

	if (kernel_cmdline) {
			realtek_organize_cmdline(kernel_cmdline, ddr_dtb_addr);
	}

out:
	if (descriptors != NULL) {
		avb_free(descriptors);
	}
	return ret;
}

void efuse_write_zeros(int vbmeta_desc_rollback, u8 *new_vbmeta_rollback)
{
	int i, j;
	int zero_a = 0;
	int zero_b = 0;

	zero_a = vbmeta_desc_rollback / 8;
	zero_b = vbmeta_desc_rollback % 8;
	for (i = 0; i < ROLLBACK_EFUSE_BYTES; i++) {
		if (i < zero_a) {
			*(new_vbmeta_rollback + i) = 0;
		} else if (i == zero_a) {
			*(new_vbmeta_rollback + i) = 0xFF;
			for (j = 0; j < zero_b; j++) {
				*(new_vbmeta_rollback + i) &= (0xFE << j);
			}
		} else {
			*(new_vbmeta_rollback + i) =  0xFF;
		}
	}

}

int efuse_count_zeros(u8 *efuse_vbmeta_rollback)
{
	int i, j;
	int rollback = 0;
	u8 tmp;

	for (i = 0; i < ROLLBACK_EFUSE_BYTES; i++) {
		tmp = ~(*(efuse_vbmeta_rollback + i));
		for (j = 0; j < 8; j++) {
			if (tmp & (1 << j)) {
				rollback++;
			}
		}
	}

	return rollback;
}

/* Efuse 0x390~0x3B0 is for VBMeta rollback index. */
/* The maximum version is 256(0x100). */
int realtek_linux_anti_rollback(u8 *vbmeta_version)
{
	int ret = CMD_RET_SUCCESS;
	struct udevice *dev = NULL;
	u8 buf[ROLLBACK_EFUSE_BYTES];
	int efuse_vbmeta_rollback = 0;
	int vbmeta_desc_rollback = 0;

	if ((*(vbmeta_version + ROLLBACK_INDEX_BYTES - 2) == 1) && !(*(vbmeta_version + ROLLBACK_INDEX_BYTES - 1))) {
		vbmeta_desc_rollback = 256;
	} else if (*(vbmeta_version + ROLLBACK_INDEX_BYTES - 2) > 1) {
		printf("Error: Invalid VBMeta version, 32 OTP bytes are reserved for VBMeta rollback index with max value 256\n");
		return CMD_RET_FAILURE;
	} else {
		vbmeta_desc_rollback = *(vbmeta_version + ROLLBACK_INDEX_BYTES - 1);
	}

	ret = uclass_get_device_by_name(UCLASS_MISC, "otp@0", &dev);
	misc_read(dev, EFUSE_ROLLBACK_INDEX_OFFSET, buf, ROLLBACK_EFUSE_BYTES);
	efuse_vbmeta_rollback = efuse_count_zeros(buf);
	if (efuse_vbmeta_rollback == vbmeta_desc_rollback) {
		return CMD_RET_SUCCESS;
	} else if (efuse_vbmeta_rollback < vbmeta_desc_rollback) {
		u8 new_efuse_rollback[ROLLBACK_EFUSE_BYTES];
		ret = CMD_RET_SUCCESS;
		printf("New VBMeta version provided\n");
		efuse_write_zeros(vbmeta_desc_rollback, new_efuse_rollback);
		ret = misc_write(dev, EFUSE_ROLLBACK_INDEX_OFFSET, new_efuse_rollback, ROLLBACK_EFUSE_BYTES);
		if (ret < 0) {
			return CMD_RET_FAILURE;
		}
		return CMD_RET_SUCCESS;
	} else {
		printf("Error: VBMeta version is too old\n");
		return CMD_RET_FAILURE;
	}
}

int realtek_linux_verified_boot(
	unsigned long ddr_kernel_addr, unsigned long ddr_dtb_addr,
	const u8* vbmeta_buf, u64 vbmeta_len,
	const u8* cert_buf, u64 cert_buf_len)
{
	AvbVBMetaVerifyResult vb_ret;
	int i, ret = CMD_RET_SUCCESS;
	const uint8_t* public_key;
	size_t public_key_len;
	AvbSHA256Ctx sha256_ctx;
	uint8_t* cal_digest = NULL;
	u8 vbmeta_version[ROLLBACK_INDEX_BYTES];

	/* vbmeta signature verify. */
	vb_ret = avb_vbmeta_image_verify(vbmeta_buf, vbmeta_len, &public_key, &public_key_len);
	/* public key hash verify. */
	avb_sha256_init(&sha256_ctx);
	avb_sha256_update(&sha256_ctx, public_key, public_key_len);
	cal_digest = avb_sha256_final(&sha256_ctx);
	if (memcmp(cert_buf + CERT_VBMETA_PK_HASH_OFFSET, cal_digest, AVB_SHA256_DIGEST_SIZE)) {
		printf("Error: Public key hash has been doctored\n");
		return CMD_RET_FAILURE;
	} else {
		printf("Public key hash verified success\n");
	}

	/* rollback index verify. */
	for (i = 0; i < ROLLBACK_INDEX_BYTES; i++) {
		vbmeta_version[i] = *(vbmeta_buf + VBMETA_ROLLBACK_INDEX_OFFSET + i);
	}
	if (realtek_linux_anti_rollback(vbmeta_version) != CMD_RET_SUCCESS) {
		printf("Error: Wrong image version, boot has been rejected by rollback protection\n");
		return CMD_RET_FAILURE;
	} else {
		printf("Rollback index verified success\n");
	}

	/* kernel and dtb verify. */
	switch (vb_ret) {
	case AVB_VBMETA_VERIFY_RESULT_OK:
		printf("VBMeta signature verified success\n");
		if (realtek_avb_support(ddr_kernel_addr, ddr_dtb_addr, vbmeta_buf, vbmeta_len) == CMD_RET_SUCCESS){
			avb_flag = 1;
			return CMD_RET_SUCCESS;
		} else {
			return CMD_RET_FAILURE;
		}
	case AVB_VBMETA_VERIFY_RESULT_OK_NOT_SIGNED:
		printf("Warning: Image is not signed, boot in non-secure mode\n");
		return ret;
	case AVB_VBMETA_VERIFY_RESULT_INVALID_VBMETA_HEADER:
	case AVB_VBMETA_VERIFY_RESULT_UNSUPPORTED_VERSION:
	case AVB_VBMETA_VERIFY_RESULT_HASH_MISMATCH:
	case AVB_VBMETA_VERIFY_RESULT_SIGNATURE_MISMATCH:
	default:
		printf("Error: Image is broken, boot failed\n");
		return CMD_RET_FAILURE;
	}
}
