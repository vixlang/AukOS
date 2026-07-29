# curl — source files for AukOS port
#
# STATUS: Framework only — requires TCP in kernel and DNS resolution.
# Currently AukOS only supports UDP (SOCK_DGRAM).
#
# This is a reference list. Start with lib/urlapi.c + lib/curl_ctype.c
# and add files one by one as missing symbols are identified.
#
# To generate full list from upstream tree:
#   cd user/curl && find lib src -name '*.c' ! -name '*amiga*' \
#     ! -name '*_win32*' ! -name '*_gssapi*' ! -name '*_sspi*' \
#     ! -name '*c_ares*' ! -name '*hyper*' | sort

CURL_LIB_SOURCES := \
	lib/bufref.c \
	lib/cf-socket.c \
	lib/cfilters.c \
	lib/conncache.c \
	lib/connect.c \
	lib/curl_addrinfo.c \
	lib/curl_des.c \
	lib/curl_endian.c \
	lib/curl_fnmatch.c \
	lib/curl_get_line.c \
	lib/curl_gethostname.c \
	lib/curl_getenv.c \
	lib/curl_memrchr.c \
	lib/curl_multibyte.c \
	lib/curl_ntlm_core.c \
	lib/curl_path.c \
	lib/curl_range.c \
	lib/curl_sasl.c \
	lib/curl_sspi.c \
	lib/curl_threads.c \
	lib/dict.c \
	lib/doh.c \
	lib/dynbuf.c \
	lib/dynhds.c \
	lib/easy.c \
	lib/easy_getopt.c \
	lib/easy_lock.c \
	lib/easyoptions.c \
	lib/escape.c \
	lib/file.c \
	lib/fileinfo.c \
	lib/fopen.c \
	lib/formdata.c \
	lib/ftp.c \
	lib/ftplistparser.c \
	lib/getenv.c \
	lib/getinfo.c \
	lib/gopher.c \
	lib/hash.c \
	lib/headers.c \
	lib/hmac.c \
	lib/hostcheck.c \
	lib/hostip.c \
	lib/hostip4.c \
	lib/hostip6.c \
	lib/hsts.c \
	lib/http.c \
	lib/http1.c \
	lib/http2.c \
	lib/http_aws_sigv4.c \
	lib/http_chunks.c \
	lib/http_digest.c \
	lib/http_negotiate.c \
	lib/http_ntlm.c \
	lib/http_proxy.c \
	lib/httpproxy.c \
	lib/if2ip.c \
	lib/imap.c \
	lib/inet_ntop.c \
	lib/inet_pton.c \
	lib/krb5.c \
	lib/ldap.c \
	lib/llist.c \
	lib/md4.c \
	lib/md5.c \
	lib/memdebug.c \
	lib/mime.c \
	lib/mprintf.c \
	lib/mqtt.c \
	lib/multi.c \
	lib/netrc.c \
	lib/nonblock.c \
	lib/noproxy.c \
	lib/openldap.c \
	lib/parsedate.c \
	lib/pingpong.c \
	lib/pop3.c \
	lib/progress.c \
	lib/psl.c \
	lib/rand.c \
	lib/rename.c \
	lib/rtsp.c \
	lib/select.c \
	lib/sendf.c \
	lib/setopt.c \
	lib/sha256.c \
	lib/share.c \
	lib/slist.c \
	lib/smb.c \
	lib/smtp.c \
	lib/socketpair.c \
	lib/socks.c \
	lib/speedcheck.c \
	lib/splay.c \
	lib/strcase.c \
	lib/strdup.c \
	lib/strerror.c \
	lib/strtok.c \
	lib/strtoofft.c \
	lib/telnet.c \
	lib/tftp.c \
	lib/timeval.c \
	lib/transfer.c \
	lib/url.c \
	lib/urlapi.c \
	lib/version.c \
	lib/warnless.c \
	lib/ws.c \
	lib/x509asn1.c

CURL_TOOL_SOURCES := \
	src/tool_binmode.c \
	src/tool_bname.c \
	src/tool_cb_dbg.c \
	src/tool_cb_hdr.c \
	src/tool_cb_prg.c \
	src/tool_cb_rea.c \
	src/tool_cb_see.c \
	src/tool_cb_wrt.c \
	src/tool_cfgable.c \
	src/tool_dirhie.c \
	src/tool_filetime.c \
	src/tool_findfile.c \
	src/tool_formparse.c \
	src/tool_getparam.c \
	src/tool_getpass.c \
	src/tool_help.c \
	src/tool_helpers.c \
	src/tool_hugehelp.c \
	src/tool_listhelp.c \
	src/tool_main.c \
	src/tool_msgs.c \
	src/tool_operate.c \
	src/tool_operhlp.c \
	src/tool_paramhlp.c \
	src/tool_progress.c \
	src/tool_sdecls.c \
	src/tool_select.c \
	src/tool_setopt.c \
	src/tool_sleep.c \
	src/tool_stderr.c \
	src/tool_strdup.c \
	src/tool_switch.c \
	src/tool_timefun.c \
	src/tool_urlglob.c \
	src/tool_util.c \
	src/tool_writeout.c \
	src/tool_writeout_json.c \
	src/tool_xattr.c

$(eval $(call port_objects,$(CURL_LIB_SOURCES) $(CURL_TOOL_SOURCES)))
