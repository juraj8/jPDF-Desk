#pragma once

#include "jpdf_desk/document/signature_timestamp.h"
#include "jpdf_desk/crypto/validation_time.h"
#include <openssl/pkcs7.h>

SignatureTimestamp verifySignatureTimestamp(PKCS7 *signature,
                                             const jpdf::crypto::ValidationTime &time);
