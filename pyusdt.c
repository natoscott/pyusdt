#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "usdt.h"

/* Helper to extract code object info */
static int get_code_info(PyObject *code_obj, const char **func_name, const char **filename, int *lineno)
{
	PyObject *name = PyObject_GetAttrString(code_obj, "co_name");
	PyObject *file = PyObject_GetAttrString(code_obj, "co_filename");
	PyObject *line = PyObject_GetAttrString(code_obj, "co_firstlineno");

	if (!name || !file || !line) {
		Py_XDECREF(name);
		Py_XDECREF(file);
		Py_XDECREF(line);
		return -1;
	}

	*func_name = PyUnicode_AsUTF8(name);
	*filename = PyUnicode_AsUTF8(file);
	*lineno = PyLong_AsLong(line);

	Py_DECREF(name);
	Py_DECREF(file);
	Py_DECREF(line);

	if (!*func_name || !*filename || *lineno == -1) {
		if (PyErr_Occurred())
			return -1;
	}

	return 0;
}

/* PY_START and PY_RESUME: callback(code, instruction_offset) */
static PyObject *py_start_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;

	if (!PyArg_ParseTuple(args, "Ol", &code_obj, &offset))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	USDT(pyusdt, PY_START, function_name, filename, line_number, offset);
	Py_RETURN_NONE;
}

static PyObject *py_resume_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;

	if (!PyArg_ParseTuple(args, "Ol", &code_obj, &offset))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	USDT(pyusdt, PY_RESUME, function_name, filename, line_number, offset);
	Py_RETURN_NONE;
}

/* PY_RETURN and PY_YIELD: callback(code, instruction_offset, retval) */
static PyObject *py_return_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	PyObject *retval;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;
	const char *retval_repr;

	if (!PyArg_ParseTuple(args, "OlO", &code_obj, &offset, &retval))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	/* Get string representation of return value */
	PyObject *repr = PyObject_Repr(retval);
	if (repr) {
		retval_repr = PyUnicode_AsUTF8(repr);
		if (retval_repr) {
			USDT(pyusdt, PY_RETURN, function_name, filename, line_number, offset, retval_repr);
		}
		Py_DECREF(repr);
	} else {
		/* Clear the error and continue */
		PyErr_Clear();
	}

	Py_RETURN_NONE;
}

static PyObject *py_yield_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	PyObject *retval;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;
	const char *retval_repr;

	if (!PyArg_ParseTuple(args, "OlO", &code_obj, &offset, &retval))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	/* Get string representation of yielded value */
	PyObject *repr = PyObject_Repr(retval);
	if (repr) {
		retval_repr = PyUnicode_AsUTF8(repr);
		if (retval_repr) {
			USDT(pyusdt, PY_YIELD, function_name, filename, line_number, offset, retval_repr);
		}
		Py_DECREF(repr);
	} else {
		/* Clear the error and continue */
		PyErr_Clear();
	}

	Py_RETURN_NONE;
}

/* CALL: callback(code, instruction_offset, callable, arg0) */
static PyObject *call_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	PyObject *callable;
	PyObject *arg0;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;
	const char *callable_repr;

	if (!PyArg_ParseTuple(args, "OlOO", &code_obj, &offset, &callable, &arg0))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &line_number) < 0)
		return NULL;

	/* Get string representation of callable */
	PyObject *repr = PyObject_Repr(callable);
	if (repr) {
		callable_repr = PyUnicode_AsUTF8(repr);
		if (callable_repr) {
			USDT(pyusdt, CALL, function_name, filename, line_number, offset, callable_repr);
		}
		Py_DECREF(repr);
	} else {
		/* Clear the error and continue */
		PyErr_Clear();
	}

	Py_RETURN_NONE;
}

/* LINE: callback(code, line_number) */
static PyObject *line_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	int line_number;
	const char *function_name;
	const char *filename;
	int first_line;

	if (!PyArg_ParseTuple(args, "Oi", &code_obj, &line_number))
		return NULL;

	if (get_code_info(code_obj, &function_name, &filename, &first_line) < 0)
		return NULL;

	USDT(pyusdt, LINE, function_name, filename, line_number);
	Py_RETURN_NONE;
}

static PyMethodDef PyUSDTMethods[] = {
	{"_py_start_callback", py_start_callback, METH_VARARGS, "PY_START callback"},
	{"_py_resume_callback", py_resume_callback, METH_VARARGS, "PY_RESUME callback"},
	{"_py_return_callback", py_return_callback, METH_VARARGS, "PY_RETURN callback"},
	{"_py_yield_callback", py_yield_callback, METH_VARARGS, "PY_YIELD callback"},
	{"_call_callback", call_callback, METH_VARARGS, "CALL callback"},
	{"_line_callback", line_callback, METH_VARARGS, "LINE callback"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef pyusdtmodule = {
	PyModuleDef_HEAD_INIT,
	"libpyusdt",
	"USDT probe support for Python profiling",
	-1,
	PyUSDTMethods
};

/* Helper function to register a single event callback */
static int register_event_callback(PyObject *module, PyObject *monitoring, int tool_id,
                                     const char *event_name, const char *callback_name)
{
	PyObject *events;
	PyObject *event;
	PyObject *callback;
	PyObject *result;

	/* Get events object */
	events = PyObject_GetAttrString(monitoring, "events");
	if (!events)
		return -1;

	/* Get specific event */
	event = PyObject_GetAttrString(events, event_name);
	Py_DECREF(events);
	if (!event)
		return -1;

	/* Get callback function from module */
	callback = PyObject_GetAttrString(module, callback_name);
	if (!callback) {
		Py_DECREF(event);
		return -1;
	}

	/* Register callback */
	result = PyObject_CallMethod(monitoring, "register_callback", "iOO", tool_id, event, callback);
	Py_DECREF(callback);
	Py_DECREF(event);

	if (!result)
		return -1;

	Py_DECREF(result);
	return 0;
}

PyMODINIT_FUNC PyInit_libpyusdt(void)
{
	PyObject *module;
	PyObject *sys_module;
	PyObject *monitoring;
	PyObject *profiler_id;
	PyObject *events;
	PyObject *result;
	int tool_id;
	int event_mask;

	module = PyModule_Create(&pyusdtmodule);
	if (module == NULL)
		return NULL;

	/* Import sys.monitoring */
	sys_module = PyImport_ImportModule("sys");
	if (!sys_module) {
		Py_DECREF(module);
		return NULL;
	}

	monitoring = PyObject_GetAttrString(sys_module, "monitoring");
	Py_DECREF(sys_module);
	if (!monitoring) {
		Py_DECREF(module);
		return NULL;
	}

	/* Get PROFILER_ID */
	profiler_id = PyObject_GetAttrString(monitoring, "PROFILER_ID");
	if (!profiler_id) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}
	tool_id = PyLong_AsLong(profiler_id);
	Py_DECREF(profiler_id);

	/* Register tool */
	result = PyObject_CallMethod(monitoring, "use_tool_id", "is", tool_id, "pyusdt-profiling");
	if (!result) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}
	Py_DECREF(result);

	/* Get events and calculate event mask (PY_START | PY_RESUME | PY_RETURN | PY_YIELD | CALL | LINE) */
	events = PyObject_GetAttrString(monitoring, "events");
	if (!events) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}

	/* Build event mask: 1 | 2 | 4 | 8 | 16 | 32 = 63 */
	event_mask = 0;
	PyObject *py_start = PyObject_GetAttrString(events, "PY_START");
	PyObject *py_resume = PyObject_GetAttrString(events, "PY_RESUME");
	PyObject *py_return = PyObject_GetAttrString(events, "PY_RETURN");
	PyObject *py_yield = PyObject_GetAttrString(events, "PY_YIELD");
	PyObject *call = PyObject_GetAttrString(events, "CALL");
	PyObject *line = PyObject_GetAttrString(events, "LINE");

	if (py_start) event_mask |= PyLong_AsLong(py_start);
	if (py_resume) event_mask |= PyLong_AsLong(py_resume);
	if (py_return) event_mask |= PyLong_AsLong(py_return);
	if (py_yield) event_mask |= PyLong_AsLong(py_yield);
	if (call) event_mask |= PyLong_AsLong(call);
	if (line) event_mask |= PyLong_AsLong(line);

	Py_XDECREF(py_start);
	Py_XDECREF(py_resume);
	Py_XDECREF(py_return);
	Py_XDECREF(py_yield);
	Py_XDECREF(call);
	Py_XDECREF(line);
	Py_DECREF(events);

	/* Set all events at once */
	result = PyObject_CallMethod(monitoring, "set_events", "ii", tool_id, event_mask);
	if (!result) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}
	Py_DECREF(result);

	/* Register callbacks for each event */
	if (register_event_callback(module, monitoring, tool_id, "PY_START", "_py_start_callback") < 0 ||
	    register_event_callback(module, monitoring, tool_id, "PY_RESUME", "_py_resume_callback") < 0 ||
	    register_event_callback(module, monitoring, tool_id, "PY_RETURN", "_py_return_callback") < 0 ||
	    register_event_callback(module, monitoring, tool_id, "PY_YIELD", "_py_yield_callback") < 0 ||
	    register_event_callback(module, monitoring, tool_id, "CALL", "_call_callback") < 0 ||
	    register_event_callback(module, monitoring, tool_id, "LINE", "_line_callback") < 0) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}

	Py_DECREF(monitoring);

	PySys_WriteStderr("pyusdt monitoring enabled (PY_START, PY_RESUME, PY_RETURN, PY_YIELD, CALL, LINE)\n");

	return module;
}
