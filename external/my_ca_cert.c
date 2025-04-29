/* 用户自定义证书 */
#ifndef MY_CUSTOM_CERT
#define MY_CUSTOM_CERT \
    "-----BEGIN CERTIFICATE-----\r\n" \
    "MIIDjTCCAnWgAwIBAgIUCAs96kTOrkFlgY2NFaF4g7NocPYwDQYJKoZIhvcNAQEL\r\n" \
    "BQAwVTELMAkGA1UEBhMCQ04xCzAJBgNVBAgMAkpTMQswCQYDVQQHDAJOSjEQMA4G\r\n" \
    "A1UECgwHY29tcGFueTEMMAoGA1UECwwDSW9UMQwwCgYDVQQDDANJb1QwIBcNMjUw\r\n" \
    "NDI4MDQwMTI3WhgPMjA1MjA5MTMwNDAxMjdaMFUxCzAJBgNVBAYTAkNOMQswCQYD\r\n" \
    "VQQIDAJKUzELMAkGA1UEBwwCTkoxEDAOBgNVBAoMB2NvbXBhbnkxDDAKBgNVBAsM\r\n" \
    "A0lvVDEMMAoGA1UEAwwDSW9UMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC\r\n" \
    "AQEA4BsfxKUz3/Sk2LXNT7fXbdH054EpjLC1ngtsQHsxTdDzs1oDUOvNxC7JUXVu\r\n" \
    "LJswpie/gSkqV/syXGf5RG/2aaEnCK6KoeBGwk9QELTTPmBmPmuyOBCA1eVgLGbB\r\n" \
    "vIv33Z7UwF0gb16RENughqHaqhvnqpIxfnyCfq7g5cd0f+8Nj4MyKKH94ryQwplw\r\n" \
    "iCuFo/58wfS/zLUfds+xH/1dyOeV2XRp+nFpGHL9U0C3wBP9xS1V71SJOPXTeibE\r\n" \
    "0/neN/zP/usHJMhu6zg7o4ZUUfWzt6cxsy+ivsmZQgDX2AZNKsCCEJg/rGwF6u95\r\n" \
    "6rd3/mShmikIaogsaxQHXQYp1wIDAQABo1MwUTAdBgNVHQ4EFgQUVPRn6Pbs38LJ\r\n" \
    "q6w4Z8qBG+DhxTMwHwYDVR0jBBgwFoAUVPRn6Pbs38LJq6w4Z8qBG+DhxTMwDwYD\r\n" \
    "VR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEA3y+bO2dy79oHpMlvJY/j\r\n" \
    "HgrXTHJQaB/7POdP+KRAOqYrqowM3R9V+mF/mb86EthL5baFqBdfb1Tx0jCUG+hp\r\n" \
    "MzEK3msPtWA3n7p6IcHMVSgLGYIKsb2V3JYanJr9926+5sZCbTL8v3bGikQ/mfBr\r\n" \
    "olU941hrGfg4t5nvzhNaNehkwjXX7j7SZcWO0J2uMN0L3+cEje5Fk121b5xu7nsR\r\n" \
    "42L0pl3/Fd4qqOQ1fVe/DdbpK3a5rNKChS7U3UChI5jz2EOgJKiJIoHrPnAq5NH3\r\n" \
    "y2T2/7Opaxk1ELh6+vAeia8VU+Zo8roD5TpP6xJrXSXGGUKMAgVe3/0HzzMc+3+7\r\n" \
    "nw==\r\n" \
    "-----END CERTIFICATE-----\r\n" 
#endif

/* 导出用户自定义证书，供其他文件使用 */
const char *my_custom_cert = MY_CUSTOM_CERT; 