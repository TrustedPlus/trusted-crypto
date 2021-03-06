#include "../stdafx.h"

#include "wrapper/pki/cert.h"

Certificate::Certificate(Handle<CertificationRequest> csr) :SSLObject<X509>(X509_new(), &so_X509_free){
	LOGGER_FN();

	try{
		X509 *x = NULL;
		X509_REQ *req = NULL;
		EVP_PKEY *pkey;
		STACK_OF(X509_EXTENSION) *exts = NULL;
		X509_EXTENSION *ext;
		ASN1_OBJECT *obj;
		int i, idx;
		int verResult;

		if (csr->isEmpty()){
			THROW_EXCEPTION(0, Certificate, NULL, "CertificationRequest empty");
		}

		req = csr->internal();

		if (req == NULL) {
			THROW_EXCEPTION(0, Certificate, NULL, "req empty");
		}

		if ((req->req_info == NULL) ||
			(req->req_info->pubkey == NULL) ||
			(req->req_info->pubkey->public_key == NULL) ||
			(req->req_info->pubkey->public_key->data == NULL)) {
			THROW_EXCEPTION(0, Certificate, NULL, "Request not contain a public key");
		}

		LOGGER_OPENSSL(X509_REQ_get_pubkey);
		if ((pkey = X509_REQ_get_pubkey(req)) == NULL) {
			THROW_EXCEPTION(0, Certificate, NULL, "Cannot get public key");
		}

		LOGGER_OPENSSL(X509_REQ_verify);
		verResult = X509_REQ_verify(req, pkey);
		LOGGER_OPENSSL(EVP_PKEY_free);
		EVP_PKEY_free(pkey);

		if (verResult < 0) {
			THROW_EXCEPTION(0, Certificate, NULL, "Signature of request verification error");
		}

		if (verResult == 0) {
			THROW_EXCEPTION(0, Certificate, NULL, "Signature did not match the certificate request");
		}

		LOGGER_OPENSSL(X509_new);
		if ((x = X509_new()) == NULL) {
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_new");
		}

		if (req->req_info->subject) {
			LOGGER_OPENSSL(X509_set_issuer_name);
			if (!X509_set_issuer_name(x, req->req_info->subject)) {
				THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_issuer_name");
			}

			LOGGER_OPENSSL(X509_set_subject_name);
			if (!X509_set_subject_name(x, req->req_info->subject)) {
				THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_subject_name");
			}
		}

		if (req->req_info->version != nullptr) {
			LOGGER_OPENSSL(X509_set_version);
			if (!X509_set_version(x, csr->getVersion())) {
				THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_version");
			}
		}

		LOGGER_OPENSSL(X509_gmtime_adj);
		X509_gmtime_adj(X509_get_notBefore(x), 0);
		LOGGER_OPENSSL(X509_time_adj_ex);
		X509_gmtime_adj(X509_get_notAfter(x), 60 * 60 * 24 * 365);


		if ((pkey = X509_REQ_get_pubkey(req)) == NULL) {
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "Cannot get public key");
		}

		LOGGER_OPENSSL(X509_set_pubkey);
		if (!X509_set_pubkey(x, pkey)) {
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_pubkey");
		}

		LOGGER_OPENSSL(EVP_PKEY_free);
		EVP_PKEY_free(pkey);

		LOGGER_OPENSSL(X509_REQ_get_extensions);
		exts = X509_REQ_get_extensions(req);

		if (exts) {
			/* Set version to V3 */
			LOGGER_OPENSSL(X509_set_version);
			if (!X509_set_version(x, 2)) {
				THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_version");
			}

			LOGGER_OPENSSL(sk_X509_EXTENSION_num);
			for (i = 0; i < sk_X509_EXTENSION_num(exts); i++) {
				LOGGER_OPENSSL(sk_X509_EXTENSION_value);
				ext = sk_X509_EXTENSION_value(exts, i);

				LOGGER_OPENSSL(X509_EXTENSION_get_object);
				obj = X509_EXTENSION_get_object(ext);

				LOGGER_OPENSSL(X509_get_ext_by_OBJ);
				idx = X509_get_ext_by_OBJ(x, obj, -1);
				if (idx != -1) {
					continue;
				}

				LOGGER_OPENSSL(X509_add_ext);
				if (!X509_add_ext(x, ext, -1)) {
					THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_add_ext");
				}
			}

			sk_X509_EXTENSION_pop_free(exts, X509_EXTENSION_free);
		}

		this->setData(x);
	}
	catch (Handle<Exception> &e){
		THROW_EXCEPTION(0, Certificate, e, "Error create certificate from request");
	}
}

Handle<Key> Certificate::getPublicKey() {
	LOGGER_FN();

	if (!this->isEmpty()) {
		LOGGER_OPENSSL(X509_get_pubkey);
		EVP_PKEY *key = X509_get_pubkey(this->internal());
		if (!key) {
			THROW_EXCEPTION(0, Certificate, NULL, "X509_get_pubkey");
		}

		return new Key(key, this->handle());
	}
	return NULL;
}

Handle<Certificate> Certificate::duplicate(){
	LOGGER_FN();

	X509 *cert = NULL;
	LOGGER_OPENSSL(X509_dup);
	cert = X509_dup(this->internal());
	if (!cert)
		THROW_EXCEPTION(1, Certificate, NULL, "X509_dup");
	return new Certificate(cert);
}

void Certificate::sign(Handle<Key> key, const char* digest){
	LOGGER_FN();

	try{
		const EVP_MD *md_alg = NULL;
		int md_type = 0;

		if (!digest) {
			LOGGER_OPENSSL(EVP_PKEY_get_default_digest_nid);
			if (EVP_PKEY_get_default_digest_nid(key->internal(), &md_type) <= 0) {
				THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "default digest for key type not found");
			}

			LOGGER_OPENSSL(EVP_get_digestbynid);
			md_alg = EVP_get_digestbynid(md_type);
		}
		else {
			LOGGER_OPENSSL(EVP_get_digestbyname);
			md_alg = EVP_get_digestbyname(digest);
		}
		
		if (!md_alg){
			THROW_EXCEPTION(0, Certificate, NULL, "Can not get digest");
		}

		LOGGER_OPENSSL(X509_sign);
		if (!X509_sign(this->internal(), key->internal(), md_alg)){
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_sign 'Error sign X509_REQ'");
		}
	}
	catch (Handle<Exception> &e){
		THROW_EXCEPTION(0, Certificate, e, "Error sign csr");
	}
}

void Certificate::read(Handle<Bio> in, DataFormat::DATA_FORMAT format){
	LOGGER_FN();

	if (in.isEmpty())
		THROW_EXCEPTION(0, Certificate, NULL, "Parameter %d cann't be NULL", 1);

	X509 *cert = NULL;

	in->reset();

	switch (format){
	case DataFormat::DER:
		LOGGER_OPENSSL(d2i_X509_bio);
		cert = d2i_X509_bio(in->internal(), NULL);
		break;
	case DataFormat::BASE64:
		LOGGER_OPENSSL(PEM_read_bio_X509);
		cert = PEM_read_bio_X509(in->internal(), NULL, NULL, NULL);
		break;
	default:
		THROW_EXCEPTION(0, Certificate, NULL, ERROR_DATA_FORMAT_UNKNOWN_FORMAT, format);
	}

	if (!cert) {
		THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "Can not read X509 data from BIO");
	}

	this->setData(cert);
}

void Certificate::write(Handle<Bio> out, DataFormat::DATA_FORMAT format){
	LOGGER_FN();

	if (out.isEmpty())
		THROW_EXCEPTION(0, Certificate, NULL, "Parameter %d is NULL", 1);

	switch (format){
	case DataFormat::DER:
		LOGGER_OPENSSL(i2d_X509_bio);
		if (i2d_X509_bio(out->internal(), this->internal()) < 1)
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "i2d_X509_bio", NULL);
		break;
	case DataFormat::BASE64:
		LOGGER_OPENSSL(PEM_write_bio_X509);
		if (PEM_write_bio_X509(out->internal(), this->internal()) < 1)
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "PEM_write_bio_X509", NULL);
		break;
	default:
		THROW_EXCEPTION(0, Certificate, NULL, ERROR_DATA_FORMAT_UNKNOWN_FORMAT, format);
	}
}

#include <openssl/cms.h>

Handle<std::string> Certificate::getSubjectFriendlyName()
{
	LOGGER_FN();

	LOGGER_OPENSSL(X509_get_subject_name);
	return GetCommonName(X509_get_subject_name(this->internal()));
}

Handle<std::string> Certificate::getIssuerFriendlyName()
{
	LOGGER_FN();

	LOGGER_OPENSSL(X509_get_issuer_name);
	return GetCommonName(X509_get_issuer_name(this->internal()));
}

Handle<std::string> Certificate::GetCommonName(X509_NAME *a){
	LOGGER_FN();

	Handle<std::string> name = new std::string("");
	if (a == NULL)
		THROW_EXCEPTION(0, Certificate, NULL, "Parameter 1 can not be NULL");

	int nid = NID_commonName;
	LOGGER_OPENSSL(X509_NAME_get_index_by_NID);
	int index = X509_NAME_get_index_by_NID(a, nid, -1);
	if (index >= 0) {
		LOGGER_OPENSSL(X509_NAME_get_entry);
		X509_NAME_ENTRY *issuerNameCommonName = X509_NAME_get_entry(a, index);

		if (issuerNameCommonName) {
			LOGGER_OPENSSL(X509_NAME_ENTRY_get_data);
			ASN1_STRING *issuerCNASN1 = X509_NAME_ENTRY_get_data(issuerNameCommonName);

			if (issuerCNASN1 != NULL) {
				unsigned char *utf = NULL;
				LOGGER_OPENSSL(ASN1_STRING_to_UTF8);
				ASN1_STRING_to_UTF8(&utf, issuerCNASN1);
				name = new std::string((char *)utf);
				OPENSSL_free(utf);
			}
		}
	}
	else {
		return new std::string("No common name");
	}


	return name;
}

Handle<std::string> Certificate::getSubjectName()
{
	LOGGER_FN();

	LOGGER_OPENSSL(X509_get_subject_name);
	X509_NAME *name = X509_get_subject_name(this->internal());
	if (!name)
		THROW_EXCEPTION(0, Certificate, NULL, "X509_NAME is NULL");

	LOGGER_OPENSSL(X509_NAME_oneline_ex);
	std::string str_name = X509_NAME_oneline_ex(name);

	Handle<std::string> res = new std::string(str_name.c_str(), str_name.length());

	return res;
}

Handle<std::string> Certificate::getIssuerName()
{
	LOGGER_FN();

	LOGGER_OPENSSL(X509_get_issuer_name);
	X509_NAME *name = X509_get_issuer_name(this->internal());
	if (!name)
		THROW_EXCEPTION(0, Certificate, NULL, "X509_NAME is NULL");

	LOGGER_OPENSSL(X509_NAME_oneline_ex);
	std::string str_name = X509_NAME_oneline_ex(name);

	Handle<std::string> res = new std::string(str_name.c_str(), str_name.length());

	return res;
}

Handle<std::string> Certificate::getNotAfter()
{
	LOGGER_FN();

	LOGGER_OPENSSL(X509_get_notAfter);
	ASN1_TIME *time = X509_get_notAfter(this->internal());
	LOGGER_OPENSSL(ASN1_TIME_to_generalizedtime);
	ASN1_GENERALIZEDTIME *gtime = ASN1_TIME_to_generalizedtime(time, NULL);
	Handle<Bio> out = new Bio(BIO_TYPE_MEM, "");
	LOGGER_OPENSSL(ASN1_GENERALIZEDTIME_print);
	ASN1_GENERALIZEDTIME_print(out->internal(), gtime);
	LOGGER_OPENSSL(ASN1_GENERALIZEDTIME_free);
	ASN1_GENERALIZEDTIME_free(gtime);
	return out->read();
}

Handle<std::string> Certificate::getNotBefore()
{
	LOGGER_FN();

	LOGGER_OPENSSL(X509_get_notBefore);
	ASN1_TIME *time = X509_get_notBefore(this->internal());
	LOGGER_OPENSSL(ASN1_TIME_to_generalizedtime);
	ASN1_GENERALIZEDTIME *gtime = ASN1_TIME_to_generalizedtime(time, NULL);
	Handle<Bio> out = new Bio(BIO_TYPE_MEM, "");
	LOGGER_OPENSSL(ASN1_GENERALIZEDTIME_print);
	ASN1_GENERALIZEDTIME_print(out->internal(), gtime);
	LOGGER_OPENSSL(ASN1_GENERALIZEDTIME_free);
	ASN1_GENERALIZEDTIME_free(gtime);
	return out->read();
}

Handle<std::string> Certificate::getSerialNumber()
{
	LOGGER_FN();

	LOGGER_OPENSSL(BIO_new);
	BIO * bioSerial = BIO_new(BIO_s_mem());
	LOGGER_OPENSSL(i2a_ASN1_INTEGER);
	if (i2a_ASN1_INTEGER(bioSerial, X509_get_serialNumber(this->internal())) < 0){
		THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "i2a_ASN1_INTEGER", NULL);
	}

	int contlen;
	char * cont;
	LOGGER_OPENSSL(BIO_get_mem_data);
	contlen = BIO_get_mem_data(bioSerial, &cont);

	Handle<std::string> res = new std::string(cont, contlen);

	BIO_free(bioSerial);

	return res;
}

Handle<std::string> Certificate::getSignatureAlgorithm() {
	LOGGER_FN();

	X509_ALGOR *sigalg = this->internal()->sig_alg;

	LOGGER_OPENSSL(X509_get_signature_nid);
	int sig_nid = X509_get_signature_nid(this->internal());
	if (sig_nid != NID_undef) {
		LOGGER_OPENSSL(OBJ_nid2ln);
		return new std::string(OBJ_nid2ln(sig_nid));
	}

	return (new Algorithm(sigalg))->getName();
}

Handle<std::string> Certificate::getSignatureDigestAlgorithm() {
	LOGGER_FN();

	int signature_nid = 0, md_nid = 0;

	LOGGER_OPENSSL("X509_get_signature_nid");
	signature_nid = X509_get_signature_nid(this->internal());
	if (!signature_nid){
		//THROW_OPENSSL_EXCEPTION(0, SignedData, NULL, "Unknown signature nid");
		return new std::string("");
	}

	LOGGER_OPENSSL("OBJ_find_sigid_algs");
	if (!OBJ_find_sigid_algs(signature_nid, &md_nid, NULL)) {
		return new std::string("");
	}

	if (!md_nid){
		THROW_OPENSSL_EXCEPTION(0, SignedData, NULL, "Unknown digest name");
	}

	return new std::string(OBJ_nid2ln(md_nid));
}

Handle<std::string> Certificate::getPublicKeyAlgorithm() {
	LOGGER_FN();

	X509_CINF *ci = this->internal()->cert_info;

	return (new Algorithm(ci->key->algor))->getName();
}

Handle<std::string> Certificate::getOrganizationName(){
	LOGGER_FN();

	X509_NAME * a = NULL;
	Handle<std::string> organizationName = new std::string("");
	if ((a = X509_get_subject_name(this->internal())) == NULL) {
		THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "Cannot get subject name");
	}

	int nid = NID_organizationName;
	LOGGER_OPENSSL(X509_NAME_get_index_by_NID);
	int index = X509_NAME_get_index_by_NID(a, nid, -1);
	if (index >= 0) {
		LOGGER_OPENSSL(X509_NAME_get_entry);
		X509_NAME_ENTRY *subjectNameOrganizationName = X509_NAME_get_entry(a, index);

		if (subjectNameOrganizationName) {
			LOGGER_OPENSSL(X509_NAME_ENTRY_get_data);
			ASN1_STRING *organizationCNASN1 = X509_NAME_ENTRY_get_data(subjectNameOrganizationName);

			if (organizationCNASN1 != NULL) {
				unsigned char *utf = NULL;
				LOGGER_OPENSSL(ASN1_STRING_to_UTF8);
				ASN1_STRING_to_UTF8(&utf, organizationCNASN1);
				organizationName = new std::string((char *)utf);
				OPENSSL_free(utf);
			}
		}
	}
	else {
		return new std::string("");
	}

	return organizationName;
}

int Certificate::compare(Handle<Certificate> cert){
	LOGGER_FN();

	LOGGER_OPENSSL(X509_cmp);
	int res = X509_cmp(this->internal(), cert->internal());

	return res;
}

Handle<std::string> Certificate::getThumbprint()
{
	LOGGER_FN();

	return this->hash(EVP_sha1());
}

long Certificate::getVersion(){
	LOGGER_FN();

	LOGGER_OPENSSL(X509_get_version);
	long res = X509_get_version(this->internal());

	return res;
}

int Certificate::getType(){
	LOGGER_FN();

	LOGGER_OPENSSL(X509_get_pubkey);
	EVP_PKEY *pk = X509_get_pubkey(this->internal());
	if (!pk)
		THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_get_pubkey", NULL);

	return pk->type;
}

int Certificate::getKeyUsage(){
	LOGGER_FN();

	LOGGER_OPENSSL(X509_check_purpose);
	X509_check_purpose(this->internal(), -1, -1);
	if (this->internal()->ex_flags & EXFLAG_KUSAGE)
		return this->internal()->ex_kusage;

	return UINT32_MAX;
}

std::vector<std::string> Certificate::getOCSPUrls() {
	LOGGER_FN();

	std::vector<std::string> res;
	const char *OCSPUrl = NULL;

	try {
		STACK_OF(ACCESS_DESCRIPTION)* pStack = NULL;
		LOGGER_OPENSSL(X509_get_ext_d2i);
		pStack = (STACK_OF(ACCESS_DESCRIPTION)*) X509_get_ext_d2i(this->internal(), NID_info_access, NULL, NULL);
		if (pStack){
			LOGGER_OPENSSL(sk_ACCESS_DESCRIPTION_num);
			for (int j = 0; j < sk_ACCESS_DESCRIPTION_num(pStack); j++){
				LOGGER_OPENSSL(sk_ACCESS_DESCRIPTION_value);
				ACCESS_DESCRIPTION *pRes = (ACCESS_DESCRIPTION *)sk_ACCESS_DESCRIPTION_value(pStack, j);
				if (pRes != NULL && pRes->method != NULL && OBJ_obj2nid(pRes->method) == NID_ad_OCSP){
					GENERAL_NAME *pName = pRes->location;
					if (pName != NULL && pName->type == GEN_URI) {
						LOGGER_OPENSSL(ASN1_STRING_data);
						OCSPUrl = (const char *)ASN1_STRING_data(pName->d.uniformResourceIdentifier);
						res.push_back(OCSPUrl);
					}
				}
			}

			LOGGER_OPENSSL(sk_ACCESS_DESCRIPTION_free);
			sk_ACCESS_DESCRIPTION_free(pStack);
		}
	}
	catch (Handle<Exception> &e){
		THROW_EXCEPTION(0, Certificate, e, "Error get OCSP urls");
	}

	return res;
}

std::vector<std::string> Certificate::getCAIssuersUrls() {
	LOGGER_FN();

	std::vector<std::string> res;
	const char *CAIssuerUrl = NULL;

	try {
		STACK_OF(ACCESS_DESCRIPTION)* pStack = NULL;
		LOGGER_OPENSSL(X509_get_ext_d2i);
		pStack = (STACK_OF(ACCESS_DESCRIPTION)*) X509_get_ext_d2i(this->internal(), NID_info_access, NULL, NULL);
		if (pStack){
			LOGGER_OPENSSL(sk_ACCESS_DESCRIPTION_num);
			for (int j = 0; j < sk_ACCESS_DESCRIPTION_num(pStack); j++){
				LOGGER_OPENSSL(sk_ACCESS_DESCRIPTION_value);
				ACCESS_DESCRIPTION *pRes = (ACCESS_DESCRIPTION *)sk_ACCESS_DESCRIPTION_value(pStack, j);
				if (pRes != NULL && pRes->method != NULL && OBJ_obj2nid(pRes->method) == NID_ad_ca_issuers){
					GENERAL_NAME *pName = pRes->location;
					if (pName != NULL && pName->type == GEN_URI) {
						LOGGER_OPENSSL(ASN1_STRING_data);
						CAIssuerUrl = (const char *)ASN1_STRING_data(pName->d.uniformResourceIdentifier);
						res.push_back(CAIssuerUrl);
					}
				}
			}

			LOGGER_OPENSSL(sk_ACCESS_DESCRIPTION_free);
			sk_ACCESS_DESCRIPTION_free(pStack);
		}
	}
	catch (Handle<Exception> &e){
		THROW_EXCEPTION(0, Certificate, e, "Error get CA issuers urls");
	}

	return res;
}

Handle<ExtensionCollection> Certificate::getExtensions() {
	LOGGER_FN();

	X509_EXTENSIONS *exts = NULL;
	X509_CINF *ci;

	ci = this->internal()->cert_info;

	if (ci) {
		exts = ci->extensions;
	}

	return new ExtensionCollection(exts);
}

void Certificate::setSubject(Handle<std::string> xName) {
	LOGGER_FN();

	try{
		if (xName.isEmpty()){
			THROW_EXCEPTION(0, Certificate, NULL, "Parameter 1 can not be NULL");
		}

		LOGGER_OPENSSL(X509_NAME_new);
		X509_NAME *name = X509_NAME_new();

		std::string strName = xName->c_str();
		strName = strName + "/";

		std::string sl = "/";
		std::string eq = "=";

		size_t pos = 0, posInBuf = 0;

		std::string buf, field, param;

		while ((pos = strName.find(sl)) != std::string::npos)  {
			buf = strName.substr(0, pos);
			if (buf.length() > 0){
				posInBuf = buf.find(eq);
				field = buf.substr(0, posInBuf);
				param = buf.substr(posInBuf + 1, buf.length());

				LOGGER_OPENSSL(X509_NAME_add_entry_by_txt);
				if (!X509_NAME_add_entry_by_txt(name, field.c_str(), MBSTRING_UTF8, (const unsigned char *)param.c_str(), -1, -1, 0)){
					THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_NAME_add_entry_by_txt 'Unable add param to X509_NAME'");
				}
			}
			strName.erase(0, pos + sl.length());
		}

		LOGGER_OPENSSL(X509_set_subject_name);
		if (!X509_set_subject_name(this->internal(), name)) {
			X509_NAME_free(name);

			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_subject_name 'Error set subject name'");
		}

		if (name){
			LOGGER_OPENSSL(X509_NAME_free);
			X509_NAME_free(name);
		}
	}
	catch (Handle<Exception> &e){
		THROW_EXCEPTION(0, Certificate, e, "Error set subject to X509");
	}
}

void Certificate::setIssuer(Handle<std::string> xName) {
	LOGGER_FN();

	try{
		if (xName.isEmpty()){
			THROW_EXCEPTION(0, Certificate, NULL, "Parameter 1 can not be NULL");
		}

		LOGGER_OPENSSL(X509_NAME_new);
		X509_NAME *name = X509_NAME_new();

		std::string strName = xName->c_str();
		strName = strName + "/";

		std::string sl = "/";
		std::string eq = "=";

		size_t pos = 0, posInBuf = 0;

		std::string buf, field, param;

		while ((pos = strName.find(sl)) != std::string::npos)  {
			buf = strName.substr(0, pos);
			if (buf.length() > 0){
				posInBuf = buf.find(eq);
				field = buf.substr(0, posInBuf);
				param = buf.substr(posInBuf + 1, buf.length());

				LOGGER_OPENSSL(X509_NAME_add_entry_by_txt);
				if (!X509_NAME_add_entry_by_txt(name, field.c_str(), MBSTRING_UTF8, (const unsigned char *)param.c_str(), -1, -1, 0)){
					THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_NAME_add_entry_by_txt 'Unable add param to X509_NAME'");
				}
			}
			strName.erase(0, pos + sl.length());
		}

		LOGGER_OPENSSL(X509_set_issuer_name);
		if (!X509_set_issuer_name(this->internal(), name)) {
			X509_NAME_free(name);

			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_issuer_name 'Error set subject name'");
		}

		if (name){
			LOGGER_OPENSSL(X509_NAME_free);
			X509_NAME_free(name);
		}
	}
	catch (Handle<Exception> &e){
		THROW_EXCEPTION(0, Certificate, e, "Error set issuer to X509");
	}
}

void Certificate::setVersion(long version){
	LOGGER_FN();

	LOGGER_OPENSSL(X509_set_version);
	if (!X509_set_version(this->internal(), version)) {
		THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_version");
	}
}

void Certificate::setNotBefore(long offset_sec){
	LOGGER_FN();

	LOGGER_OPENSSL(X509_gmtime_adj);
	X509_gmtime_adj(X509_get_notBefore(this->internal()), offset_sec);
}

void Certificate::setNotAfter(long offset_sec){
	LOGGER_FN();

	LOGGER_OPENSSL(X509_gmtime_adj);
	X509_gmtime_adj(X509_get_notAfter(this->internal()), offset_sec);
}

void Certificate::setExtensions(Handle<ExtensionCollection> exts) {
	LOGGER_FN();

	try {
		X509_EXTENSIONS *xexts = NULL;
		X509_EXTENSION *ext = NULL;
		ASN1_OBJECT *obj;
		int i, idx;

		if (exts.isEmpty()) {
			THROW_EXCEPTION(0, Certificate, NULL, "Extensions is empty");
		}

		if (!(xexts = exts->internal())) {
			THROW_EXCEPTION(0, Certificate, NULL, "Extensions is empty");
		}

		LOGGER_OPENSSL(sk_X509_EXTENSION_num);
		for (i = 0; i < sk_X509_EXTENSION_num(xexts); i++) {
			LOGGER_OPENSSL(sk_X509_EXTENSION_value);
			ext = sk_X509_EXTENSION_value(xexts, i);

			LOGGER_OPENSSL(X509_EXTENSION_get_object);
			obj = X509_EXTENSION_get_object(ext);

			LOGGER_OPENSSL(X509_get_ext_by_OBJ);
			idx = X509_get_ext_by_OBJ(this->internal(), obj, -1);
			if (idx != -1) {
				continue;
			}

			LOGGER_OPENSSL(X509_add_ext);
			if (!X509_add_ext(this->internal(), ext, -1)) {
				THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_add_ext");
			}
		}

		sk_X509_EXTENSION_pop_free(xexts, X509_EXTENSION_free);

		return;
	}
	catch (Handle<Exception> &e) {
		THROW_EXCEPTION(0, CertificationRequest, e, "Error set extensions");
	}
}

void Certificate::setSerialNumber(Handle<std::string> serial){
	LOGGER_FN();

	ASN1_INTEGER *sno = NULL;
	BIGNUM *btmp;

	if (serial.isEmpty() || !serial->length()) {
		LOGGER_OPENSSL(ASN1_INTEGER_new);
		sno = ASN1_INTEGER_new();

		LOGGER_OPENSSL(BN_new);
		if (!(btmp = BN_new())) {
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "BN_new");
		}

		LOGGER_OPENSSL(BN_pseudo_rand);
		if (!BN_pseudo_rand(btmp, SERIAL_RAND_BITS, 0, 0)) {
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "BN_pseudo_rand");
		}

		LOGGER_OPENSSL(BN_to_ASN1_INTEGER);
		if (!BN_to_ASN1_INTEGER(btmp, sno)) {
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "BN_to_ASN1_INTEGER");
		}

		LOGGER_OPENSSL(BN_free);
		BN_free(btmp);
	}
	else {
		LOGGER_OPENSSL(s2i_ASN1_INTEGER);
		if (!(sno = s2i_ASN1_INTEGER(NULL, (char *)serial->c_str()))) {
			THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "s2i_ASN1_INTEGER");
		}
	}

	LOGGER_OPENSSL(X509_set_serialNumber);
	if (!X509_set_serialNumber(this->internal(), sno)) {
		THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_set_serialNumber");
	}

	LOGGER_OPENSSL(ASN1_INTEGER_free);
	ASN1_INTEGER_free(sno);

	return;
}

bool Certificate::equals(Handle<Certificate> cert){
	LOGGER_FN();

	Handle<std::string> cert1 = this->getThumbprint();
	Handle<std::string> cert2 = cert->getThumbprint();

	if (cert1->compare(*cert2) == 0){
		return true;
	}
	return false;
}

bool Certificate::isSelfSigned() {
	LOGGER_FN();

	LOGGER_OPENSSL(X509_check_purpose);
	X509_check_purpose(this->internal(), -1, 0);
	if (this->internal()->ex_flags & EXFLAG_SS) {
		return true;
	}
	else {
		return false;
	}
}

bool Certificate::isCA() {
	LOGGER_FN();

	LOGGER_OPENSSL(X509_check_ca);
	return (X509_check_ca(this->internal()) > 0);
}

Handle<std::string> Certificate::hash(Handle<std::string> algorithm){
	LOGGER_FN();

	LOGGER_OPENSSL(EVP_get_digestbyname);
	const EVP_MD *md = EVP_get_digestbyname(algorithm->c_str());
	if (!md) {
		THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "EVP_get_digestbyname");
	}

	return this->hash(md);
}

Handle<std::string> Certificate::hash(const EVP_MD *md) {
	LOGGER_FN();

	unsigned char hash[EVP_MAX_MD_SIZE] = { 0 };
	unsigned int hashlen = 0;

	LOGGER_OPENSSL(X509_digest);
	if (!X509_digest(this->internal(), md, hash, &hashlen)) {
		THROW_OPENSSL_EXCEPTION(0, Certificate, NULL, "X509_digest");
	}

	Handle<std::string> res = new std::string((char *)hash, hashlen);

	return res;
}