SUMMARY = "WpeQt Qt/WPE browser packagegroup"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit packagegroup

RDEPENDS:${PN} += " \
    wpeqt \
"