SUMMARY = "systemd-networkd + wpa_supplicant configuration for picam"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://20-wired.network \
    file://25-wireless.network \
"
S = "${WORKDIR}"

inherit allarch

# Đặt trong kas/wifi.yml (không commit mật khẩu lên git)
WIFI_SSID ?= ""
WIFI_PSK ?= ""
WIFI_COUNTRY ?= "VN"

do_install() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 ${WORKDIR}/20-wired.network ${D}${sysconfdir}/systemd/network/
    install -m 0644 ${WORKDIR}/25-wireless.network ${D}${sysconfdir}/systemd/network/

    if [ -z "${WIFI_SSID}" ]; then
        bbwarn "WIFI_SSID is empty: image will not join any WiFi network (create kas/wifi.yml)"
        return
    fi

    install -d ${D}${sysconfdir}/wpa_supplicant
    cat > ${D}${sysconfdir}/wpa_supplicant/wpa_supplicant-wlan0.conf <<'WPAEOF'
ctrl_interface=/run/wpa_supplicant
update_config=1
country=${WIFI_COUNTRY}

network={
    ssid="${WIFI_SSID}"
    psk="${WIFI_PSK}"
    key_mgmt=WPA-PSK
}
WPAEOF
    chmod 0600 ${D}${sysconfdir}/wpa_supplicant/wpa_supplicant-wlan0.conf

    # Bật wpa_supplicant@wlan0.service (template unit do gói wpa-supplicant cung cấp)
    install -d ${D}${sysconfdir}/systemd/system/multi-user.target.wants
    ln -sf ${systemd_system_unitdir}/wpa_supplicant@.service \
        ${D}${sysconfdir}/systemd/system/multi-user.target.wants/wpa_supplicant@wlan0.service
}

FILES:${PN} = "${sysconfdir}"
RDEPENDS:${PN} = "wpa-supplicant"
