#if LIBCPER_PYTHON
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <json.h>
#include <libcper/cper-parse.h>
#include <libcper/cpad-parse.h>

//Recursively converts a json-c object into the equivalent Python object.
PyObject *convert_to_pydict(json_object *jso)
{
	PyObject *ret = Py_None;
	enum json_type type = json_object_get_type(jso);
	switch (type) {
	case json_type_null:
		ret = Py_None;
		break;
	case json_type_boolean: {
		int b = json_object_get_boolean(jso);
		ret = PyBool_FromLong(b);
	} break;
	case json_type_double: {
		double d = json_object_get_double(jso);
		ret = PyFloat_FromDouble(d);
	} break;

	case json_type_int: {
		//Values may be stored as either int64 or uint64. json-c
		//saturates get_int64 to INT64_MAX for uint64 values that don't
		//fit, so fall back to the unsigned getter in that case to
		//preserve full 64-bit values (e.g. operation/address fields).
		int64_t i = json_object_get_int64(jso);
		if (i == INT64_MAX) {
			uint64_t u = json_object_get_uint64(jso);
			ret = PyLong_FromUnsignedLongLong(u);
		} else {
			ret = PyLong_FromLongLong(i);
		}
	} break;
	case json_type_object: {
		ret = PyDict_New();

		json_object_object_foreach(jso, key1, val)
		{
			PyObject *pyobj = convert_to_pydict(val);
			if (key1 != NULL) {
				if (pyobj == NULL) {
					pyobj = Py_None;
				}
				PyDict_SetItemString(ret, key1, pyobj);
				//PyDict_SetItemString does not steal a reference.
				if (pyobj != Py_None) {
					Py_DECREF(pyobj);
				}
			}
		}
	} break;
	case json_type_array: {
		ret = PyList_New(0);
		int arraylen = json_object_array_length(jso);

		for (int i = 0; i < arraylen; i++) {
			json_object *val = json_object_array_get_idx(jso, i);
			PyObject *pyobj = convert_to_pydict(val);
			if (pyobj == NULL) {
				pyobj = Py_None;
			}
			PyList_Append(ret, pyobj);
			if (pyobj != Py_None) {
				Py_DECREF(pyobj);
			}
		}
	} break;
	case json_type_string: {
		const char *strval = json_object_get_string(jso);
		ret = PyUnicode_FromString(strval);
	} break;
	}
	return ret;
}

//Recursively converts a Python object into the equivalent json-c object.
//Returns a new reference (owned by the caller); sets a Python exception and
//returns NULL on error. A NULL return with no exception set represents a
//JSON null.
static json_object *convert_from_pyobj(PyObject *obj)
{
	if (obj == NULL || obj == Py_None) {
		return NULL;
	}

	if (PyBool_Check(obj)) {
		return json_object_new_boolean(obj == Py_True);
	}

	if (PyLong_Check(obj)) {
		int overflow = 0;
		long long ll = PyLong_AsLongLongAndOverflow(obj, &overflow);
		if (overflow == 0 && !(ll == -1 && PyErr_Occurred())) {
			return json_object_new_int64((int64_t)ll);
		}
		PyErr_Clear();
		unsigned long long ull = PyLong_AsUnsignedLongLong(obj);
		if (PyErr_Occurred()) {
			return NULL;
		}
		return json_object_new_uint64((uint64_t)ull);
	}

	if (PyFloat_Check(obj)) {
		return json_object_new_double(PyFloat_AsDouble(obj));
	}

	if (PyUnicode_Check(obj)) {
		const char *s = PyUnicode_AsUTF8(obj);
		if (s == NULL) {
			return NULL;
		}
		return json_object_new_string(s);
	}

	if (PyList_Check(obj) || PyTuple_Check(obj)) {
		json_object *arr = json_object_new_array();
		Py_ssize_t n = PySequence_Size(obj);
		for (Py_ssize_t i = 0; i < n; i++) {
			PyObject *item = PySequence_GetItem(obj, i);
			json_object *child = convert_from_pyobj(item);
			int had_error = (child == NULL && PyErr_Occurred());
			Py_XDECREF(item);
			if (had_error) {
				json_object_put(arr);
				return NULL;
			}
			json_object_array_add(arr, child);
		}
		return arr;
	}

	if (PyDict_Check(obj)) {
		json_object *jobj = json_object_new_object();
		PyObject *key, *value;
		Py_ssize_t pos = 0;
		while (PyDict_Next(obj, &pos, &key, &value)) {
			const char *key_str = PyUnicode_AsUTF8(key);
			if (key_str == NULL) {
				json_object_put(jobj);
				return NULL;
			}
			json_object *child = convert_from_pyobj(value);
			if (child == NULL && PyErr_Occurred()) {
				json_object_put(jobj);
				return NULL;
			}
			json_object_object_add(jobj, key_str, child);
		}
		return jobj;
	}

	PyErr_SetString(PyExc_TypeError,
			"Unsupported type in CPER/CPAD IR object");
	return NULL;
}

//Shared helper: convert a binary buffer to a Python dict using the given
//buffer-to-IR parser.
static PyObject *parse_buf(PyObject *args,
			   json_object *(*buf_to_ir)(const unsigned char *,
						     size_t),
			   const char *errmsg)
{
	const unsigned char *data;
	Py_ssize_t count;

	if (!PyArg_ParseTuple(args, "y#", &data, &count)) {
		return NULL;
	}

	json_object *jout = buf_to_ir(data, count);
	if (jout == NULL) {
		PyErr_SetString(PyExc_ValueError, errmsg);
		return NULL;
	}

	PyObject *ret = convert_to_pydict(jout);
	json_object_put(jout);
	return ret;
}

//Shared helper: convert a Python IR object to binary using the given
//IR-to-binary serializer.
static PyObject *to_binary(PyObject *args,
			   void (*ir_to_binary)(json_object *, FILE *))
{
	PyObject *obj;
	if (!PyArg_ParseTuple(args, "O", &obj)) {
		return NULL;
	}

	json_object *ir = convert_from_pyobj(obj);
	if (ir == NULL) {
		if (!PyErr_Occurred()) {
			PyErr_SetString(PyExc_TypeError,
					"Expected a non-None IR object");
		}
		return NULL;
	}

	char *buf = NULL;
	size_t size = 0;
	FILE *stream = open_memstream(&buf, &size);
	if (stream == NULL) {
		json_object_put(ir);
		PyErr_SetString(PyExc_OSError, "open_memstream failed");
		return NULL;
	}

	ir_to_binary(ir, stream);
	fclose(stream);
	json_object_put(ir);

	PyObject *ret = PyBytes_FromStringAndSize(buf, size);
	free(buf);
	return ret;
}

static PyObject *parse(PyObject *self, PyObject *args)
{
	(void)self;
	return parse_buf(args, cper_buf_to_ir, "Failed to parse CPER buffer");
}

static PyObject *parse_cpad(PyObject *self, PyObject *args)
{
	(void)self;
	return parse_buf(args, cpad_buf_to_ir, "Failed to parse CPAD buffer");
}

static PyObject *to_cper(PyObject *self, PyObject *args)
{
	(void)self;
	return to_binary(args, ir_to_cper);
}

static PyObject *to_cpad(PyObject *self, PyObject *args)
{
	(void)self;
	return to_binary(args, ir_to_cpad);
}

static PyMethodDef methods[] = {
	{ "parse", (PyCFunction)parse, METH_VARARGS,
	  "parse(data: bytes) -> dict\n\nDecode a CPER binary record into an IR dictionary." },
	{ "parse_cpad", (PyCFunction)parse_cpad, METH_VARARGS,
	  "parse_cpad(data: bytes) -> dict\n\nDecode a CPAD binary record into an IR dictionary." },
	{ "to_cper", (PyCFunction)to_cper, METH_VARARGS,
	  "to_cper(ir: dict) -> bytes\n\nEncode a CPER IR dictionary into a binary record." },
	{ "to_cpad", (PyCFunction)to_cpad, METH_VARARGS,
	  "to_cpad(ir: dict) -> bytes\n\nEncode a CPAD IR dictionary into a binary record." },
	{ NULL, NULL, 0, NULL },
};

static struct PyModuleDef module = {
	PyModuleDef_HEAD_INIT,
	"cper",
	"Decode and encode CPER and CPAD records.",
	-1,
	methods,
	NULL,
	NULL,
	NULL,
	NULL,
};

PyMODINIT_FUNC PyInit_cper(void)
{
	return PyModule_Create(&module);
}
#endif
