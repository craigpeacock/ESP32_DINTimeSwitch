idf_component_register(SRCS "main.c" "wifi.c" "ntp.c" "aemo.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES server_root_cert.pem)
