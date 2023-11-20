/*
 * sue_fwupdate_common.h
 *
 * This file contains the common U-Boot script for the firmware upgrade process.
 */

#ifndef __SUE_FWUPDATE_COMMON_H
#define __SUE_FWUPDATE_COMMON_H

/*
 * UPDATE PROCESS is fully described at:
 *  https://confluence.streamunlimited.com/display/Stream800/SWU%3A+swupdate+documentation
 *
 * Update methods:
 *
 * METHODS:
 *              readfitUimage                   - loads uImage to RAM
 *              readswufitUimage                - loads swupdate uImage to RAM
 *              swu_boot                        - main method
 *
 * VARIABLES:
 *              bootcount                       - actual count of uncorrect reboots
 *              bootlimit                       - limit of uncorrcet reboots, if reached, board does not boot anymore
 *              swu_load_addr                   - base RAM address for u-boot operations
 *              factory_state                   - whether the board is in a factory state
 *              usb_update_req                  - whether the USB update request is active
 */

#define SUE_FWUPDATE_EXTRA_ENV_SETTINGS \
    "factory_state=0\0" \
    "usb_update_req=0\0" \
\
\
    "readfituImage=" \
        "if mtd read fit ${loadaddr}; " \
            "then " \
            "echo \"INFO: kernel partition load successful\"; " \
        "else " \
            "echo \"ERROR: cannot load kernel image from nand\"; " \
            "reset; " \
        "fi;\0" \
\
\
    "readswufituImage=" \
        "if mtd read swufit ${loadaddr}; " \
            "then " \
            "echo \"INFO: swupdate kernel partition load successful\"; " \
        "else " \
            "echo \"ERROR: cannot load swupdate kernel image from nand\"; " \
            "reset; " \
        "fi;\0" \
\
\
    "bootfitimage=" \
        "if env exists fit_config; " \
            "then " \
            "echo \"INFO: will boot fit config ${fit_config}@1\"; " \
            "bootm ${loadaddr}#${fit_config}@1; " \
        "fi; " \
        "echo \"INFO: will try to boot the default fit config\"; " \
        "bootm ${loadaddr}; " \
        "echo \"INFO: fit boot failed...\"; " \
        "echo \"INFO: resetting...\"; " \
        "reset;\0" \
\
    "kernel_common_args=setenv bootargs console=ttyS0,${baudrate} earlycon psci=enable; " \
        "if test \"${secure_board}\" = 1; " \
            "then " \
            "echo \"INFO: board is locked, booting to runlevel 3\"; " \
            "setenv bootargs ${bootargs} 3; " \
        "fi;\0" \
\
    "nandmtdpart=data\0" \
    "nandroot=ubi0:nsdk-rootfs rw\0" \
    "nandrootfstype=ubifs\0" \
    "nandargs=run kernel_common_args; " \
        "if test ${swufail} = yes; " \
            "then " \
            "setenv bootargs ${bootargs} swufail; " \
        "fi; " \
        "setenv bootargs ${bootargs} " \
	    "ubi.mtd=${nandmtdpart} root=${nandroot} noinitrd " \
	    "rootfstype=${nandrootfstype};\0" \
    "nand_boot=echo \"Booting from nand ...\"; " \
        "run nandargs; " \
        "echo \"INFO: loading fit image into RAM...\"; " \
        /*"bstate booting; "*/ \
        "run readfituImage; " \
        "echo \"INFO: booting fit image...\"; " \
        "run bootfitimage;\0" \
\
\
    "swunandargs=run kernel_common_args; " \
        "setenv bootargs ${bootargs} rootfstype=ramfs " \
        "factory_state=${factory_state} usb_update_req=${usb_update_req} " \
        "secure_board=${secure_board};\0" \
    "swu_nand_boot=echo \"Booting swu from nand ...\"; " \
        "run swunandargs; " \
        "echo \"INFO: loading swu fit image into RAM...\"; " \
        /*"bstate dontunplug; "*/ \
        "run readswufituImage; " \
        "if test \"${secure_board}\" = 0; " \
            "then " \
            "echo \"INFO: board is not locked, do not enforce swufit signature\"; " \
            "setenv verify no; " \
        "fi; " \
        "echo \"INFO: booting swu fit image...\"; " \
        "run bootfitimage;\0" \
\
\
    "check_factory_state=" \
       "echo \"INFO: Checking if fit and environment partitions are empty.\"; " \
       "setenv target_addr ${loadaddr}; " \
       "setenv factory_state 1; " \
       "mw ${loadaddr} 0xffffffff; " \
       "setexpr target_addr ${target_addr} + 4; " \
       "setexpr target_addr ${target_addr} + 4; " \
       "for part in fit environment; " \
           "do; " \
           "mtd read $part ${target_addr} 0 0x800; " \
           "cmp.l ${loadaddr} ${target_addr} 1; " \
               "if test $? -eq 1; " \
                   "then; " \
                   "setenv factory_state 0; " \
                   "echo \"INFO: partition $part is not empty.\"; " \
               "fi; " \
       "done; " \
       "if test ${factory_state} -eq 0; " \
           "then " \
           "echo \"INFO: Board is NOT in factory state.\"; " \
       "else " \
           "echo \"INFO: Board is in factory state.\"; " \
       "fi;\0" \
\
\
    "check_usb_update_request=" \
        "if fwup usb_update_req; then " \
            "echo \"INFO: USB update request is active\"; " \
            "setenv usb_update_req 1; " \
        "else " \
            "echo \"INFO: USB update request is NOT active\"; " \
            "setenv usb_update_req 0; " \
        "fi;\0" \
\
\
    "swu_boot=" \
        "if fwup fail; " \
            "then " \
            "if test \"${bootcount}\" -gt ${bootlimit}; " \
                "then " \
                "echo \"INFO: bootcount(${bootcount}) greater than bootlimit(${bootlimit})\"; " \
                /*"bstate hardfailure; "*/ \
                "echo \"ERROR: Maximum boot count reached!\"; " \
                "while true; do sleep 100; done; " \
            "fi; " \
        "fi; " \
        "boot_swupdate=no; " \
        "swufail=no; " \
        "if test ${bootcount} -eq 1; " \
            "then " \
            "echo \"INFO: Bootcount = 1, checking if board is in factory state or USB update request is active\"; " \
            "run check_factory_state; " \
            "if test ${factory_state} = 1; " \
                "then " \
                "boot_swupdate=yes; " \
            "else " \
                "run check_usb_update_request; " \
                "if test ${usb_update_req} = 1; " \
                    "then " \
                    "boot_swupdate=yes; " \
                "fi; " \
            "fi; " \
        "else " \
            "echo \"INFO: Bootcount != 1, not checking factory state or USB update request\"; " \
        "fi; " \
        "if fwup update; " \
            "then " \
            "min_boot_retry=3;" \
            "if test ${bootcount} -gt ${min_boot_retry}; " \
                "then " \
                "echo \"INFO: Update flag is set, bootcount is greater than ${min_boot_retry}\"; " \
                "swufail=yes; " \
            "else " \
                "echo \"INFO: Update flag is set but bootcount not greater than ${min_boot_retry}\"; " \
                "boot_swupdate=yes; " \
            "fi; " \
        "else " \
            "if fwup fail; " \
                "then " \
                "min_boot_retry=3;" \
                "if test ${bootcount} -gt ${min_boot_retry}; " \
                    "then " \
                    "echo \"INFO: Fail flag is set, bootcount is greater than ${min_boot_retry}\"; " \
                    "boot_swupdate=yes; " \
                "else " \
                    "echo \"INFO: Fail flag is set but bootcount not greater than ${min_boot_retry}\"; " \
                "fi; " \
            "fi; " \
        "fi; " \
        "if test ${boot_swupdate} = yes; " \
            "then " \
            "echo \"INFO: Booting swupdate image\"; " \
            /*"bstate dontunplug; "*/ \
            "run swu_nand_boot; " \
        "else " \
            "echo \"INFO: Booting regular image\"; " \
            "echo \"INFO: Setting fail flag...\"; " \
            "fwup set fail; " \
            /*"bstate normal; "*/ \
            "run nand_boot; " \
        "fi;\0" \

#define SUE_FWUPDATE_BOOTCOMMAND \
        "echo \"INFO: attempting SWU boot...\"; " \
        "fwup flags; "\
        "run swu_boot;" \

#define SUE_FWUPDATE_ALTBOOTCOMMAND \
        "echo \"ERROR: Maximum boot count reached!\"; while true; do sleep 100; done; "

#endif /* __SUE_FWUPDATE_COMMON_H */
