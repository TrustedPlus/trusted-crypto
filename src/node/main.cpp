#include "stdafx.h"

#include <openssl/cms.h>
#include <openssl/engine.h>
#include <openssl/err.h>

#include <nan.h>

#include "utils/wlog.h"

#include "pki/wkey.h"
#include "pki/wcert.h"
#include "pki/wcrl.h"
#include "pki/woid.h"
#include "pki/walg.h"
#include "pki/wattr.h"

#include <node_object_wrap.h>

void init(v8::Handle<v8::Object> target) {
	// logger.start("/tmp/trustedtls/node.log", -1); // -1 = all levels bits
	// logger.start("logger.txt", LoggerLevel::All );

	// On Windows, we can't use Node's OpenSSL, so we link
	// to a standalone OpenSSL library. Therefore, we need
	// to initialize OpenSSL separately.
	
	// TODO: Do I need to free these?
	// I'm not sure where to call ERR_free_strings() and EVP_cleanup()	
	OpenSSL::run();

	v8::Local<v8::Object> v8Pki = Nan::New<v8::Object>();

	target->Set(Nan::New("PKI").ToLocalChecked(), v8Pki);
	WCertificate::Init(v8Pki);
	WCRL::Init(v8Pki);
	WOID::Init(v8Pki);
	WAlgorithm::Init(v8Pki);
	WAttribute::Init(v8Pki);

	// target->Set(NanNew<v8::String>("utils"), NanNew<v8::Object>());
	// WLogger::Init(target->Get(NanNew<v8::String>("utils"))->ToObject());
}

NODE_MODULE(trusted, init)
