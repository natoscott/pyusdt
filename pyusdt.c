#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "usdt.h"

static PyObject *monitor_callback(PyObject *self, PyObject *args)
{
	PyObject *code_obj;
	long offset;
	const char *function_name;
	const char *filename;
	int line_number;

	/* Parse arguments: (code, offset) */
	if (!PyArg_ParseTuple(args, "Ol", &code_obj, &offset))
		return NULL;

	/* Extract code object attributes */
	PyObject *name = PyObject_GetAttrString(code_obj, "co_name");
	PyObject *file = PyObject_GetAttrString(code_obj, "co_filename");
	PyObject *lineno = PyObject_GetAttrString(code_obj, "co_firstlineno");

	if (!name || !file || !lineno) {
		Py_XDECREF(name);
		Py_XDECREF(file);
		Py_XDECREF(lineno);
		return NULL;
	}

	function_name = PyUnicode_AsUTF8(name);
	filename = PyUnicode_AsUTF8(file);
	line_number = PyLong_AsLong(lineno);

	Py_DECREF(name);
	Py_DECREF(file);
	Py_DECREF(lineno);

	if (!function_name || !filename || line_number == -1) {
		if (PyErr_Occurred())
			return NULL;
	}

	/* Fire the USDT probe */
	USDT(pyusdt, PY_START, function_name, filename, line_number);

	Py_RETURN_NONE;
}

static PyMethodDef PyUSDTMethods[] = {
	{"_monitor_callback", monitor_callback, METH_VARARGS, "Monitoring callback for PY_START events"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef pyusdtmodule = {
	PyModuleDef_HEAD_INIT,
	"libpyusdt",
	"USDT probe support for Python profiling",
	-1,
	PyUSDTMethods
};

PyMODINIT_FUNC PyInit_libpyusdt(void)
{
	PyObject *module;
	PyObject *sys_module;
	PyObject *monitoring;
	PyObject *profiler_id;
	PyObject *events;
	PyObject *py_start_event;
	PyObject *result;
	int tool_id;

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

	/* Register tool: use_tool_id(PROFILER_ID, "pyusdt-profiling") */
	result = PyObject_CallMethod(monitoring, "use_tool_id", "is", tool_id, "pyusdt-profiling");
	if (!result) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}
	Py_DECREF(result);

	/* Get events.PY_START */
	events = PyObject_GetAttrString(monitoring, "events");
	if (!events) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}

	py_start_event = PyObject_GetAttrString(events, "PY_START");
	Py_DECREF(events);
	if (!py_start_event) {
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}

	/* Set events: set_events(PROFILER_ID, events.PY_START) */
	result = PyObject_CallMethod(monitoring, "set_events", "iO", tool_id, py_start_event);
	if (!result) {
		Py_DECREF(py_start_event);
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}
	Py_DECREF(result);

	/* Get the callback function from our module */
	PyObject *callback = PyObject_GetAttrString(module, "_monitor_callback");
	if (!callback) {
		Py_DECREF(py_start_event);
		Py_DECREF(monitoring);
		Py_DECREF(module);
		return NULL;
	}

	/* Register callback: register_callback(PROFILER_ID, events.PY_START, callback) */
	result = PyObject_CallMethod(monitoring, "register_callback", "iOO", tool_id, py_start_event, callback);
	Py_DECREF(callback);
	Py_DECREF(py_start_event);
	Py_DECREF(monitoring);

	if (!result) {
		Py_DECREF(module);
		return NULL;
	}
	Py_DECREF(result);

	PySys_WriteStderr("pyusdt monitoring enabled\n");

	return module;
}
