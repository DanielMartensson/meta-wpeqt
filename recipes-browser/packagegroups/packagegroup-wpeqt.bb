SUMMARY = "WpeQt Qt/WPE browser packagegroup"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=${COMMON_LICENSE_MD5}"

inherit packagegroup

RDEPENDS:${PN} += " \
    wpeqt \
"