idf_component_register(SRCS "main.c" "gpio.c" "wifi.c" "http.c" "ntp.c" "aemo.c"
	INCLUDE_DIRS "."
	EMBED_TXTFILES server_root_cert.pem
	EMBED_TXTFILES web/index.html
	EMBED_TXTFILES web/style.css)
