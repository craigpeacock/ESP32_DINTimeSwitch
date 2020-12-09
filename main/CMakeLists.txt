idf_component_register(SRCS "esp32_DINTimeSwitch.c" "wifi.c" "ntp.c" "aemo.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES server_root_cert.pem)
