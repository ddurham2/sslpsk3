/* Copyright 2017 David R. Bild
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

// Rather than requiring the openssl headers to be found at build time, we just declare the few things from openssl/ssl.h here
//#include <openssl/ssl.h>
// .>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>.>
typedef void SSL;
typedef void SSL_CTX;

typedef unsigned int (*SSL_psk_client_cb_func)(SSL *ssl,
                                               const char *hint,
                                               char *identity,
                                               unsigned int max_identity_len,
                                               unsigned char *psk,
                                               unsigned int max_psk_len);
void SSL_CTX_set_psk_client_callback(SSL_CTX *ctx, SSL_psk_client_cb_func cb);
void SSL_set_psk_client_callback(SSL *ssl, SSL_psk_client_cb_func cb);

typedef unsigned int (*SSL_psk_server_cb_func)(SSL *ssl,
                                               const char *identity,
                                               unsigned char *psk,
                                               unsigned int max_psk_len);
void SSL_CTX_set_psk_server_callback(SSL_CTX *ctx, SSL_psk_server_cb_func cb);
void SSL_set_psk_server_callback(SSL *ssl, SSL_psk_server_cb_func cb);

int SSL_use_psk_identity_hint(SSL *s, const char *identity_hint);

void SSL_set_accept_state(SSL *s);
// <.<.<.<.<.<.<.<.<.<.<.<.<.<.<.<.<.<.<.<.<.


/* Copy PySSLObject/PySSLSocket from _ssl.c to expose the SSL*. */
#if !defined(PY_MAJOR_VERSION) || (PY_VERSION_HEX < 0x03070000)
#error Only Python 3.7 and later are supported
#endif

#define PY_VERSION_BETWEEN(start, end) ((PY_VERSION_HEX >= start) && \
                                        (PY_VERSION_HEX < end))

typedef struct { // confirmed same from python 3.7 to 3.11
    PyObject_HEAD
    PyObject *Socket; /* weakref to socket on which we're layered */
    SSL *ssl;
    /* etc ... */
} PySSLSocket;

#if PY_VERSION_BETWEEN(0x02070000, 0x03000000)
#define BYTESFMT "s"
#else
#define BYTESFMT "y"
#endif

/*
 * Python function that returns the client psk and identity.
 *
 * (ssl_id, hint) => (psk, idenity)
 */
static PyObject* python_psk_client_callback;

/*
 * Python function that returns the server psk.
 *
 * (ssl_id, identity) => psk
 */
static PyObject* python_psk_server_callback;

/*
 * Returns the index for an SSL socket, used to identity the socket across the
 * C/Python interface.
 */
long ssl_id(SSL* ssl)
{
    return (long) ssl;
}

/*
 * Called from Python to set python_psk_client_callback;
 */
PyObject* sslpsk3_set_python_psk_client_callback(PyObject* self, PyObject* args)
{
    PyObject* cb;
    if (!PyArg_ParseTuple(args, "O", &cb)) {
        return NULL;
    }
    Py_XINCREF(cb);
    Py_XDECREF(python_psk_client_callback);
    python_psk_client_callback = cb;

    Py_RETURN_NONE;
}

/*
 * Called from Python to set python_psk_server_callback;
 */
PyObject* sslpsk3_set_python_psk_server_callback(PyObject* self, PyObject* args)
{
    PyObject* cb;
    if (!PyArg_ParseTuple(args, "O", &cb)) {
        return NULL;
    }
    Py_XINCREF(cb);
    Py_XDECREF(python_psk_server_callback);
    python_psk_server_callback = cb;

    Py_RETURN_NONE;
}

/*
 * Client callback for openSSL. Delegates to python_psk_client_callback.
 */
static unsigned int sslpsk3_psk_client_callback(SSL* ssl,
                                               const char* hint,
                                               char* identity,
                                               unsigned int max_identity_len,
                                               unsigned char* psk,
                                               unsigned int max_psk_len)
{
    int ret = 0;

    PyGILState_STATE gstate;

    PyObject* result;

    const char* psk_;
    const char* identity_;

    Py_ssize_t psk_len_;
    Py_ssize_t identity_len_;

    gstate = PyGILState_Ensure();

    if (python_psk_client_callback == NULL) {
        goto release;
    }

    // Call python callback
    result = PyObject_CallFunction(python_psk_client_callback, "l"BYTESFMT, ssl_id(ssl), hint);
    if (result == NULL) {
        goto release;
    }

    // Parse result

    if (!PyArg_Parse(result, "("BYTESFMT"#"BYTESFMT"#)", &psk_, &psk_len_, &identity_, &identity_len_)) {
        goto decref;
    }

    // Copy to caller
    if (psk_len_ > max_psk_len) {
        goto decref;
    }
    memcpy(psk, psk_, psk_len_);

    if (identity_len_ + 1 > max_identity_len) {
        goto decref;
    }
    memcpy(identity, identity_, identity_len_);
    identity[identity_len_] = 0;

    ret = psk_len_;

 decref:
    Py_DECREF(result);

 release:
    PyGILState_Release(gstate);

    return ret;
}

/*
 * Server callback for openSSL. Delegates to python_psk_server_callback.
 */
static unsigned int sslpsk3_psk_server_callback(SSL* ssl,
                                               const char* identity,
                                               unsigned char* psk,
                                               unsigned int max_psk_len)
{
    int ret = 0;

    PyGILState_STATE gstate;

    PyObject* result;

    const char* psk_;
    Py_ssize_t psk_len_;

    gstate = PyGILState_Ensure();

    if (python_psk_server_callback == NULL) {
        goto release;
    }

    // Call python callback
    result = PyObject_CallFunction(python_psk_server_callback, "l"BYTESFMT, ssl_id(ssl), identity);
    if (result == NULL) {
        goto release;
    }

    // Parse result
    if (!PyArg_Parse(result, BYTESFMT"#", &psk_, &psk_len_)) {
        goto decref;
    }

    // Copy to caller
    if (psk_len_ > max_psk_len) {
        goto decref;
    }
    memcpy(psk, psk_, psk_len_);

    ret = psk_len_;

 decref:
    Py_DECREF(result);

 release:
    PyGILState_Release(gstate);

    return ret;
}

/*
 * Called from Python to set the client psk callback.
 */
PyObject* sslpsk3_set_psk_client_callback(PyObject* self, PyObject* args)
{
    PyObject* socket;
    SSL* ssl;

    if (!PyArg_ParseTuple(args, "O", &socket))
    {
        return NULL;
    }

    ssl = ((PySSLSocket*) socket)->ssl;
    SSL_set_psk_client_callback(ssl, sslpsk3_psk_client_callback);

    return Py_BuildValue("l", ssl_id(ssl));
}

/*
 * Called from Python to set the server psk callback.
 */
PyObject* sslpsk3_set_psk_server_callback(PyObject* self, PyObject* args)
{
    PyObject* socket;
    SSL* ssl;

    if (!PyArg_ParseTuple(args, "O", &socket))
    {
        return NULL;
    }

    ssl = ((PySSLSocket*) socket)->ssl;
    SSL_set_psk_server_callback(ssl, sslpsk3_psk_server_callback);

    return Py_BuildValue("l", ssl_id(ssl));
}

/*
 * Called from Python to set the server identity hint.
 */
PyObject* sslpsk3_use_psk_identity_hint(PyObject* self, PyObject* args)
{
    PyObject* socket;
    const char *hint;
    SSL* ssl;

    if (!PyArg_ParseTuple(args, "O"BYTESFMT, &socket, &hint))
    {
        return NULL;
    }

    ssl = ((PySSLSocket*) socket) ->ssl;
    SSL_use_psk_identity_hint(ssl, hint);

    return Py_BuildValue("l", ssl_id(ssl));
}

/*
 * Called from Python to place the socket into server mode
 */
PyObject* sslpsk3_set_accept_state(PyObject* self, PyObject* args)
{
    PyObject* socket;
    SSL* ssl;

    if (!PyArg_ParseTuple(args, "O", &socket))
    {
        return NULL;
    }

    ssl = ((PySSLSocket*) socket) ->ssl;
    SSL_set_accept_state(ssl);

    return Py_BuildValue("l", ssl_id(ssl));
}

static PyMethodDef sslpsk3_methods[] =
{
    {"sslpsk3_set_python_psk_client_callback", sslpsk3_set_python_psk_client_callback, METH_VARARGS, ""},
    {"sslpsk3_set_python_psk_server_callback", sslpsk3_set_python_psk_server_callback, METH_VARARGS, ""},
    {"sslpsk3_set_psk_client_callback", sslpsk3_set_psk_client_callback, METH_VARARGS, ""},
    {"sslpsk3_set_psk_server_callback", sslpsk3_set_psk_server_callback, METH_VARARGS, ""},
    {"sslpsk3_use_psk_identity_hint", sslpsk3_use_psk_identity_hint, METH_VARARGS, ""},
    {"sslpsk3_set_accept_state", sslpsk3_set_accept_state, METH_VARARGS, ""},
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef sslpsk3_moduledef = {
    PyModuleDef_HEAD_INIT,
    "sslpsk3",
    NULL,
    0,
    sslpsk3_methods,
    NULL,
    NULL,
    NULL,
    NULL
};
#endif

#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC PyInit__sslpsk3(void)
#else
void init_sslpsk3(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject* m = PyModule_Create(&sslpsk3_moduledef);
#else
    PyObject* m = Py_InitModule("_sslpsk3", sslpsk3_methods);
#endif

    if (m == NULL) {
#if PY_MAJOR_VERSION >= 3
        return NULL;
#else
        return ;
#endif
    }

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
