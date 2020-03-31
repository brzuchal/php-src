http: $(SAPI_HTTP_PATH)

$(SAPI_HTTP_PATH): $(PHP_GLOBAL_OBJS) $(PHP_BINARY_OBJS) $(PHP_FASTCGI_OBJS) $(PHP_HTTP_OBJS)
	$(BUILD_HTTP)

install-http: $(SAPI_HTTP_PATH)
	@echo "Installing PHP HTTP binary:        $(INSTALL_ROOT)$(sbindir)/"
	@$(mkinstalldirs) $(INSTALL_ROOT)$(sbindir)
	@$(mkinstalldirs) $(INSTALL_ROOT)$(localstatedir)/log
	@$(mkinstalldirs) $(INSTALL_ROOT)$(localstatedir)/run
	@$(INSTALL) -m 0755 $(SAPI_HTTP_PATH) $(INSTALL_ROOT)$(sbindir)/$(program_prefix)php-http$(program_suffix)$(EXEEXT)
