/* host_web.c - Mongoose-based web server host implementation
 *
 * Provides HTTP/WebSocket server for browser-based editing.
 * Uses mongoose library for networking.
 *
 * REST API:
 *   POST /api/run   - Execute code {code, lang}
 *   POST /api/repl  - REPL command {command, lang}
 *   POST /api/save  - Save file {filename, content}
 *   POST /api/load  - Load file {filename}
 *
 * WebSocket API:
 *   {"cmd": "load", "filename": "..."}
 *   {"cmd": "save", "filename": "...", "content": "..."}
 */

#ifdef LOKI_WEB_HOST

#include "host_web.h"
#include "host.h"
#include "session.h"
#include "event.h"
#include "json.h"
#include "jsonrpc.h"
#include "lang_bridge.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ======================= Constants ========================================= */

#define WEB_HOST_QUEUE_SIZE 256
#define WEB_HOST_DEFAULT_PORT 8080
#define WEB_HOST_POLL_MS 50
#define MAX_POST_SIZE (1024 * 1024)  /* 1MB max POST body */

/* ======================= Embedded Web UI =================================== */

/* Complete embedded xterm.js-based UI - no external files needed */
static const char *EMBEDDED_HTML =
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"  <meta charset=\"utf-8\">\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
"  <title>psnd editor</title>\n"
"  <link rel=\"stylesheet\" href=\"https://cdn.jsdelivr.net/npm/xterm@5.3.0/css/xterm.css\">\n"
"  <style>\n"
"    * { box-sizing: border-box; margin: 0; padding: 0; }\n"
"    html, body { height: 100%; background: #1e1e1e; overflow: hidden; }\n"
"    #app { display: flex; flex-direction: column; height: 100%; }\n"
"    #header { display: flex; align-items: center; gap: 12px; padding: 6px 12px;\n"
"              background: #252526; border-bottom: 1px solid #3c3c3c; }\n"
"    .logo { font-weight: bold; color: #4ec9b0; font-family: monospace; }\n"
"    .status { padding: 2px 8px; border-radius: 4px; font-size: 12px; font-family: sans-serif; }\n"
"    .status.connected { background: #4ec9b0; color: #1e1e1e; }\n"
"    .status.disconnected { background: #f44747; color: white; }\n"
"    .mode { padding: 2px 8px; border-radius: 4px; font-size: 12px; background: #569cd6;\n"
"            color: white; text-transform: uppercase; font-family: sans-serif; }\n"
"    .filename { color: #888; font-family: monospace; }\n"
"    .spacer { flex: 1; }\n"
"    .btn { padding: 4px 12px; font-size: 12px; font-family: sans-serif; background: #3c3c3c;\n"
"           color: #ccc; border: 1px solid #555; border-radius: 4px; cursor: pointer; }\n"
"    .btn:hover { background: #4c4c4c; border-color: #666; }\n"
"    .btn:active { background: #2c2c2c; }\n"
"    .btn-play { background: #2d5a2d; border-color: #3d7a3d; }\n"
"    .btn-play:hover { background: #3d6a3d; }\n"
"    .btn-stop { background: #5a2d2d; border-color: #7a3d3d; }\n"
"    .btn-stop:hover { background: #6a3d3d; }\n"
"    #terminal-wrap { flex: 1; padding: 4px; }\n"
"    #terminal { height: 100%; }\n"
"    .xterm { padding: 4px; }\n"
"    #footer { display: flex; justify-content: space-between; padding: 4px 12px;\n"
"              background: #252526; border-top: 1px solid #3c3c3c; font-size: 12px;\n"
"              color: #888; font-family: monospace; }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <div id=\"app\">\n"
"    <header id=\"header\">\n"
"      <span class=\"logo\">psnd</span>\n"
"      <span id=\"status\" class=\"status disconnected\">Disconnected</span>\n"
"      <span id=\"mode\" class=\"mode\"></span>\n"
"      <span id=\"filename\" class=\"filename\"></span>\n"
"      <span class=\"spacer\"></span>\n"
"      <button id=\"btn-play\" class=\"btn btn-play\">Play</button>\n"
"      <button id=\"btn-stop\" class=\"btn btn-stop\">Stop</button>\n"
"      <button id=\"btn-eval\" class=\"btn\">Eval</button>\n"
"    </header>\n"
"    <div id=\"terminal-wrap\"><div id=\"terminal\"></div></div>\n"
"    <footer id=\"footer\">\n"
"      <span id=\"message\"></span>\n"
"      <span id=\"position\"></span>\n"
"    </footer>\n"
"  </div>\n"
"  <script src=\"https://cdn.jsdelivr.net/npm/xterm@5.3.0/lib/xterm.min.js\"></script>\n"
"  <script src=\"https://cdn.jsdelivr.net/npm/xterm-addon-fit@0.8.0/lib/xterm-addon-fit.min.js\"></script>\n"
"  <script>\n"
"(function() {\n"
"  var statusEl = document.getElementById('status');\n"
"  var modeEl = document.getElementById('mode');\n"
"  var filenameEl = document.getElementById('filename');\n"
"  var messageEl = document.getElementById('message');\n"
"  var positionEl = document.getElementById('position');\n"
"\n"
"  var term = new Terminal({ cursorBlink: true, fontSize: 14,\n"
"    fontFamily: \"'Consolas', 'Monaco', 'Courier New', monospace\",\n"
"    theme: { background: '#1e1e1e', foreground: '#cccccc', cursor: '#cccccc',\n"
"             selection: 'rgba(255,255,255,0.3)', black: '#1e1e1e', red: '#f44747',\n"
"             green: '#4ec9b0', yellow: '#cca700', blue: '#569cd6', magenta: '#c586c0',\n"
"             cyan: '#9cdcfe', white: '#d4d4d4', brightBlack: '#808080' }});\n"
"  var fitAddon = new FitAddon.FitAddon();\n"
"  term.loadAddon(fitAddon);\n"
"  term.open(document.getElementById('terminal'));\n"
"  fitAddon.fit();\n"
"\n"
"  var ws = null, reconnectTimeout = null;\n"
"  var HL_COLORS = { 0:'\\x1b[0m', 1:'\\x1b[36m', 2:'\\x1b[36m', 3:'\\x1b[33m',\n"
"    4:'\\x1b[32m', 5:'\\x1b[35m', 6:'\\x1b[31m', 7:'\\x1b[34m', 8:'\\x1b[34m',\n"
"    9:'\\x1b[35m', 10:'\\x1b[33m', 11:'\\x1b[36m', 12:'\\x1b[32m', 13:'\\x1b[31m',\n"
"    14:'\\x1b[37m', 15:'\\x1b[35m' };\n"
"\n"
"  function setStatus(text, state) {\n"
"    statusEl.textContent = text;\n"
"    statusEl.className = 'status ' + (state || 'disconnected');\n"
"  }\n"
"\n"
"  function connect() {\n"
"    if (ws && ws.readyState === WebSocket.OPEN) return;\n"
"    setStatus('Connecting...', 'connecting');\n"
"    var proto = location.protocol === 'https:' ? 'wss:' : 'ws:';\n"
"    ws = new WebSocket(proto + '//' + location.host + '/ws');\n"
"    ws.onopen = function() {\n"
"      setStatus('Connected', 'connected');\n"
"      send({cmd: 'resize', rows: term.rows, cols: term.cols});\n"
"      send({cmd: 'snapshot'});\n"
"    };\n"
"    ws.onclose = function() {\n"
"      setStatus('Disconnected', 'disconnected'); ws = null;\n"
"      if (!reconnectTimeout) reconnectTimeout = setTimeout(function() {\n"
"        reconnectTimeout = null; connect();\n"
"      }, 2000);\n"
"    };\n"
"    ws.onerror = function() { setStatus('Error', 'disconnected'); };\n"
"    ws.onmessage = function(e) {\n"
"      try { handleMessage(JSON.parse(e.data)); } catch(err) {}\n"
"    };\n"
"  }\n"
"\n"
"  function send(obj) { if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj)); }\n"
"\n"
"  function handleMessage(msg) {\n"
"    var vm = msg.viewmodel || (msg.type === 'update' ? msg.viewmodel : null);\n"
"    if (vm) renderViewModel(vm);\n"
"  }\n"
"\n"
"  function renderViewModel(vm) {\n"
"    if (!vm) return;\n"
"    term.reset();\n"
"    if (vm.rows_content) {\n"
"      for (var i = 0; i < vm.rows_content.length; i++) {\n"
"        var row = vm.rows_content[i], line = '';\n"
"        if (vm.gutter_width > 0 && row.row_num > 0) {\n"
"          var ln = String(row.row_num);\n"
"          while (ln.length < vm.gutter_width - 1) ln = ' ' + ln;\n"
"          line += '\\x1b[90m' + ln + ' \\x1b[0m';\n"
"        } else if (vm.gutter_width > 0) {\n"
"          var sp = ''; for (var k = 0; k < vm.gutter_width - 1; k++) sp += ' ';\n"
"          line += '\\x1b[90m' + sp + '~ \\x1b[0m';\n"
"        }\n"
"        if (row.segments) {\n"
"          var lastHl = -1;\n"
"          for (var j = 0; j < row.segments.length; j++) {\n"
"            var seg = row.segments[j];\n"
"            if (seg.hl_type !== lastHl) { line += HL_COLORS[seg.hl_type] || '\\x1b[0m'; lastHl = seg.hl_type; }\n"
"            if (seg.selected) line += '\\x1b[7m';\n"
"            line += seg.text || '';\n"
"            if (seg.selected) line += '\\x1b[27m';\n"
"          }\n"
"          line += '\\x1b[0m';\n"
"        }\n"
"        if (i < vm.rows_content.length - 1) term.writeln(line); else term.write(line);\n"
"      }\n"
"    }\n"
"    if (vm.cursor && vm.cursor.visible) {\n"
"      var col = vm.cursor.col + (vm.gutter_width || 0);\n"
"      term.write('\\x1b[' + vm.cursor.row + ';' + col + 'H');\n"
"    }\n"
"    if (vm.status) {\n"
"      modeEl.textContent = vm.status.mode || '';\n"
"      filenameEl.textContent = (vm.status.filename || '[No Name]') + (vm.status.dirty ? ' [+]' : '');\n"
"      positionEl.textContent = 'Ln ' + (vm.status.current_row || 1) + ' / ' + (vm.status.numrows || 1);\n"
"    }\n"
"    if (vm.message) messageEl.textContent = vm.message;\n"
"  }\n"
"\n"
"  var SPECIAL_KEYS = { 'Escape': 27, 'Enter': 13, 'Backspace': 127, 'Tab': 9,\n"
"    'ArrowLeft': 1000, 'ArrowRight': 1001, 'ArrowUp': 1002, 'ArrowDown': 1003,\n"
"    'Delete': 1008, 'Home': 1009, 'End': 1010, 'PageUp': 1011, 'PageDown': 1012 };\n"
"\n"
"  term.onKey(function(e) {\n"
"    var key = e.key, dom = e.domEvent;\n"
"    var mods = 0;\n"
"    if (dom.ctrlKey) mods |= 1;\n"
"    if (dom.altKey) mods |= 2;\n"
"    if (dom.shiftKey) mods |= 4;\n"
"    var code;\n"
"    if (SPECIAL_KEYS[dom.key] !== undefined) code = SPECIAL_KEYS[dom.key];\n"
"    else if (key.length === 1) {\n"
"      code = key.charCodeAt(0);\n"
"      if (dom.ctrlKey && code >= 97 && code <= 122) code = code - 96;\n"
"      else if (dom.ctrlKey && code >= 65 && code <= 90) code = code - 64;\n"
"    } else return;\n"
"    if (dom.ctrlKey) dom.preventDefault();\n"
"    send({cmd: 'event', type: 'key', code: code, modifiers: mods});\n"
"  });\n"
"\n"
"  document.addEventListener('keydown', function(e) {\n"
"    if (e.ctrlKey && !e.altKey && !e.metaKey) {\n"
"      var k = e.key.toLowerCase();\n"
"      if (k === 'p' || k === 'e' || k === 's' || k === 'q' || k === 'f' || k === 'g' || k === 'r') {\n"
"        e.preventDefault();\n"
"      }\n"
"    }\n"
"  });\n"
"\n"
"  window.addEventListener('resize', function() {\n"
"    fitAddon.fit();\n"
"    send({cmd: 'resize', rows: term.rows, cols: term.cols});\n"
"    send({cmd: 'snapshot'});\n"
"  });\n"
"\n"
"  document.getElementById('terminal').addEventListener('contextmenu', function(e) { e.preventDefault(); });\n"
"  document.getElementById('terminal').addEventListener('click', function() { term.focus(); });\n"
"\n"
"  document.getElementById('btn-play').addEventListener('click', function() {\n"
"    send({cmd: 'event', type: 'key', code: 16, modifiers: 1}); term.focus();\n"
"  });\n"
"  document.getElementById('btn-stop').addEventListener('click', function() {\n"
"    send({cmd: 'event', type: 'key', code: 3, modifiers: 1}); term.focus();\n"
"  });\n"
"  document.getElementById('btn-eval').addEventListener('click', function() {\n"
"    send({cmd: 'event', type: 'key', code: 5, modifiers: 1}); term.focus();\n"
"  });\n"
"\n"
"  connect();\n"
"  term.focus();\n"
"})();\n"
"  </script>\n"
"</body>\n"
"</html>\n";

/* ======================= Web Host Data ===================================== */

typedef struct {
    struct mg_mgr mgr;              /* Mongoose event manager */
    struct mg_connection *listener; /* HTTP listener connection */
    struct mg_connection *ws_conn;  /* Active WebSocket connection (single client) */
    EditorSession *session;         /* Editor session (set by host loop) */

    /* Event queue from WebSocket */
    EditorEvent queue[WEB_HOST_QUEUE_SIZE];
    int queue_head;
    int queue_tail;
    pthread_mutex_t queue_mutex;

    /* Configuration */
    char *web_root;                 /* Static file directory (owned, may be NULL) */
    int port;                       /* Listening port */
    int running;                    /* Continue running flag */

    /* Render state */
    int needs_render;               /* Flag to push update to client */

    /* REPL state */
    char current_lang[32];          /* Current language for REPL (e.g., "alda", "joy") */
} WebHostData;

/* ======================= Event Queue ======================================= */

static int web_host_queue_event(WebHostData *data, const EditorEvent *event) {
    pthread_mutex_lock(&data->queue_mutex);

    int next_tail = (data->queue_tail + 1) % WEB_HOST_QUEUE_SIZE;
    if (next_tail == data->queue_head) {
        pthread_mutex_unlock(&data->queue_mutex);
        return -1; /* Queue full */
    }

    data->queue[data->queue_tail] = *event;
    data->queue_tail = next_tail;

    pthread_mutex_unlock(&data->queue_mutex);
    return 0;
}

static int web_host_dequeue_event(WebHostData *data, EditorEvent *event) {
    pthread_mutex_lock(&data->queue_mutex);

    if (data->queue_head == data->queue_tail) {
        pthread_mutex_unlock(&data->queue_mutex);
        return -1; /* Queue empty */
    }

    *event = data->queue[data->queue_head];
    data->queue_head = (data->queue_head + 1) % WEB_HOST_QUEUE_SIZE;

    pthread_mutex_unlock(&data->queue_mutex);
    return 0;
}

/* ======================= Response Helpers ================================== */

static void send_json_response(struct mg_connection *c, int status, const char *json) {
    mg_http_reply(c, status, "Content-Type: application/json\r\n", "%s", json);
}

static void send_json_ok(struct mg_connection *c, const char *extra) {
    if (extra) {
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                     "{\"ok\":true,%s}", extra);
    } else {
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                     "{\"ok\":true}");
    }
}

static void send_json_error(struct mg_connection *c, int status, const char *error) {
    JsonBuilder jb;
    json_builder_init(&jb);
    json_object_start(&jb);
    json_kv_bool(&jb, "ok", 0);
    json_kv_string(&jb, "error", error);
    json_object_end(&jb);
    send_json_response(c, status, json_builder_get(&jb));
    json_builder_free(&jb);
}

/* ======================= REST API Handlers ================================= */

/* POST /api/run - Execute code */
static void handle_api_run(struct mg_connection *c, struct mg_http_message *hm, WebHostData *data) {
    if (!data->session) {
        send_json_error(c, 503, "Session not ready");
        return;
    }

    /* Parse request body */
    char *body = malloc(hm->body.len + 1);
    if (!body) {
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';

    JsonValue req = json_parse(body);
    free(body);

    if (req.type == JSON_ERROR) {
        send_json_error(c, 400, "Invalid JSON");
        return;
    }

    const char *code = json_object_get_string(&req, "code");
    const char *lang = json_object_get_string(&req, "lang");

    if (!code) {
        json_value_free(&req);
        send_json_error(c, 400, "Missing 'code' parameter");
        return;
    }

    /* Check for first-line language directive: #alda, #joy, etc. */
    const char *code_start = code;
    char detected_lang[32] = {0};
    if (code[0] == '#') {
        /* Extract language name from first line */
        const char *end = code + 1;
        while (*end && *end != '\n' && *end != '\r' && *end != ' ' && (end - code) < 31) {
            end++;
        }
        size_t len = end - (code + 1);
        if (len > 0 && len < sizeof(detected_lang)) {
            memcpy(detected_lang, code + 1, len);
            detected_lang[len] = '\0';
            /* Skip to next line for actual code */
            while (*end && (*end == ' ' || *end == '\t')) end++;
            if (*end == '\n') end++;
            else if (*end == '\r') { end++; if (*end == '\n') end++; }
            code_start = end;
        }
    }

    /* Get filename from session for language detection */
    const char *filename = editor_session_get_filename(data->session);

    /* Try to find language: explicit param > first-line directive > file extension > default */
    const LokiLangOps *ops = NULL;
    if (lang && *lang) {
        ops = loki_lang_by_name(lang);
    }
    if (!ops && detected_lang[0]) {
        ops = loki_lang_by_name(detected_lang);
    }
    if (!ops && filename) {
        ops = loki_lang_for_file(filename);
    }
    /* Fall back to first available language */
    if (!ops) {
        int lang_count = 0;
        const LokiLangOps **langs = loki_lang_all(&lang_count);
        if (lang_count > 0) {
            ops = langs[0];
        }
    }

    /* Get editor context for language evaluation */
    editor_ctx_t *ctx = editor_session_get_ctx(data->session);

    int result = -1;
    const char *error = NULL;

    if (ops && ops->eval && ctx) {
        /* Initialize language if needed */
        if (ops->init && (!ops->is_initialized || !ops->is_initialized(ctx))) {
            ops->init(ctx);
        }
        result = ops->eval(ctx, code_start);  /* Use code_start to skip directive */
        if (result != 0 && ops->get_error) {
            error = ops->get_error(ctx);
        }
    } else {
        error = "No language available for this file type";
    }

    json_value_free(&req);

    if (result == 0) {
        send_json_ok(c, "\"output\":\"\"");
    } else {
        send_json_error(c, 200, error ? error : "Execution failed");
    }
}

/* POST /api/repl - REPL command */
static void handle_api_repl(struct mg_connection *c, struct mg_http_message *hm, WebHostData *data) {
    if (!data->session) {
        send_json_error(c, 503, "Session not ready");
        return;
    }

    /* Parse request body */
    char *body = malloc(hm->body.len + 1);
    if (!body) {
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';

    JsonValue req = json_parse(body);
    free(body);

    if (req.type == JSON_ERROR) {
        send_json_error(c, 400, "Invalid JSON");
        return;
    }

    const char *command = json_object_get_string(&req, "command");
    const char *lang = json_object_get_string(&req, "lang");

    if (!command) {
        json_value_free(&req);
        send_json_error(c, 400, "Missing 'command' parameter");
        return;
    }

    /* Handle ex-commands (starting with :) */
    if (command[0] == ':') {
        const char *cmd = command + 1;  /* Skip the colon */

        /* :help - show available commands */
        if (strcmp(cmd, "help") == 0) {
            int lang_count = 0;
            const LokiLangOps **langs = loki_lang_all(&lang_count);

            /* Build help message with available languages */
            char help[512];
            int offset = snprintf(help, sizeof(help),
                "Commands:\\n"
                "  :help     - Show this help\\n"
                "  :lang     - Show current language\\n"
                "  :langs    - List available languages\\n"
                "  :<name>   - Switch to language (");

            for (int i = 0; i < lang_count && offset < (int)sizeof(help) - 20; i++) {
                offset += snprintf(help + offset, sizeof(help) - offset,
                    "%s%s", i > 0 ? ", " : "", langs[i]->name);
            }
            snprintf(help + offset, sizeof(help) - offset, ")");

            json_value_free(&req);
            /* Escape for JSON */
            char response[600];
            snprintf(response, sizeof(response), "\"output\":\"%s\"", help);
            send_json_ok(c, response);
            return;
        }

        /* :lang - show current language */
        if (strcmp(cmd, "lang") == 0) {
            char response[128];
            snprintf(response, sizeof(response), "\"output\":\"Current language: %s\"",
                data->current_lang[0] ? data->current_lang : "alda (default)");
            json_value_free(&req);
            send_json_ok(c, response);
            return;
        }

        /* :langs - list available languages */
        if (strcmp(cmd, "langs") == 0) {
            int lang_count = 0;
            const LokiLangOps **langs = loki_lang_all(&lang_count);

            char list[256] = "Available languages: ";
            int offset = strlen(list);
            for (int i = 0; i < lang_count && offset < (int)sizeof(list) - 20; i++) {
                offset += snprintf(list + offset, sizeof(list) - offset,
                    "%s%s", i > 0 ? ", " : "", langs[i]->name);
            }

            char response[300];
            snprintf(response, sizeof(response), "\"output\":\"%s\"", list);
            json_value_free(&req);
            send_json_ok(c, response);
            return;
        }

        /* Try to match a language name (e.g., :alda, :joy) */
        const LokiLangOps *lang_ops = loki_lang_by_name(cmd);
        if (lang_ops) {
            strncpy(data->current_lang, cmd, sizeof(data->current_lang) - 1);
            data->current_lang[sizeof(data->current_lang) - 1] = '\0';

            char response[128];
            snprintf(response, sizeof(response), "\"output\":\"Switched to %s\"", cmd);
            json_value_free(&req);
            send_json_ok(c, response);
            return;
        }

        /* Unknown command */
        json_value_free(&req);
        send_json_error(c, 200, "Unknown command. Type :help for available commands.");
        return;
    }

    /* Get filename from session for language detection */
    const char *filename = editor_session_get_filename(data->session);

    /* Determine language: explicit param > stored REPL lang > file extension > default */
    const LokiLangOps *ops = NULL;
    if (lang && *lang) {
        ops = loki_lang_by_name(lang);
    }
    if (!ops && data->current_lang[0]) {
        ops = loki_lang_by_name(data->current_lang);
    }
    if (!ops && filename) {
        ops = loki_lang_for_file(filename);
    }
    /* Fall back to first available language (usually alda) */
    if (!ops) {
        int lang_count = 0;
        const LokiLangOps **langs = loki_lang_all(&lang_count);
        if (lang_count > 0) {
            ops = langs[0];
        }
    }

    /* Get editor context for language evaluation */
    editor_ctx_t *ctx = editor_session_get_ctx(data->session);

    int result = -1;
    const char *error = NULL;

    if (ops && ops->eval && ctx) {
        if (ops->init && (!ops->is_initialized || !ops->is_initialized(ctx))) {
            ops->init(ctx);
        }
        result = ops->eval(ctx, command);
        if (result != 0 && ops->get_error) {
            error = ops->get_error(ctx);
        }
    } else {
        error = "No language available";
    }

    json_value_free(&req);

    if (result == 0) {
        send_json_ok(c, "\"output\":\"\"");
    } else {
        send_json_error(c, 200, error ? error : "Command failed");
    }
}

/* POST /api/save - Save file */
static void handle_api_save(struct mg_connection *c, struct mg_http_message *hm, WebHostData *data) {
    (void)data; /* May be used later for session access */

    /* Parse request body */
    char *body = malloc(hm->body.len + 1);
    if (!body) {
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';

    JsonValue req = json_parse(body);
    free(body);

    if (req.type == JSON_ERROR) {
        send_json_error(c, 400, "Invalid JSON");
        return;
    }

    const char *filename = json_object_get_string(&req, "filename");
    const char *content = json_object_get_string(&req, "content");

    if (!filename || !content) {
        json_value_free(&req);
        send_json_error(c, 400, "Missing 'filename' or 'content' parameter");
        return;
    }

    /* Write file */
    FILE *f = fopen(filename, "w");
    if (!f) {
        json_value_free(&req);
        send_json_error(c, 500, "Failed to open file for writing");
        return;
    }

    size_t written = fwrite(content, 1, strlen(content), f);
    fclose(f);

    json_value_free(&req);

    if (written > 0) {
        send_json_ok(c, NULL);
    } else {
        send_json_error(c, 500, "Failed to write file");
    }
}

/* POST /api/load - Load file */
static void handle_api_load(struct mg_connection *c, struct mg_http_message *hm, WebHostData *data) {
    (void)data;

    /* Parse request body */
    char *body = malloc(hm->body.len + 1);
    if (!body) {
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }
    memcpy(body, hm->body.buf, hm->body.len);
    body[hm->body.len] = '\0';

    JsonValue req = json_parse(body);
    free(body);

    if (req.type == JSON_ERROR) {
        send_json_error(c, 400, "Invalid JSON");
        return;
    }

    const char *filename = json_object_get_string(&req, "filename");
    if (!filename) {
        json_value_free(&req);
        send_json_error(c, 400, "Missing 'filename' parameter");
        return;
    }

    /* Read file */
    FILE *f = fopen(filename, "r");
    if (!f) {
        json_value_free(&req);
        send_json_error(c, 404, "File not found");
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        json_value_free(&req);
        send_json_error(c, 500, "Memory allocation failed");
        return;
    }

    size_t read = fread(content, 1, size, f);
    content[read] = '\0';
    fclose(f);

    /* Build response */
    JsonBuilder jb;
    json_builder_init(&jb);
    json_object_start(&jb);
    json_kv_bool(&jb, "ok", 1);
    json_kv_string(&jb, "filename", filename);
    json_kv_string(&jb, "content", content);
    json_object_end(&jb);

    send_json_response(c, 200, json_builder_get(&jb));

    json_builder_free(&jb);
    free(content);
    json_value_free(&req);
}

/* ======================= WebSocket Message Processing ====================== */

/* Parse WebSocket JSON and queue appropriate event or handle command */
static void web_host_process_message(WebHostData *data, struct mg_connection *c,
                                     const char *msg, size_t len) {
    /* Parse JSON */
    char *json_str = malloc(len + 1);
    if (!json_str) return;
    memcpy(json_str, msg, len);
    json_str[len] = '\0';

    JsonValue cmd = json_parse(json_str);
    free(json_str);

    if (cmd.type == JSON_ERROR) {
        return;
    }

    const char *cmd_type = json_object_get_string(&cmd, "cmd");
    if (!cmd_type) {
        json_value_free(&cmd);
        return;
    }

    EditorEvent event = {0};

    if (strcmp(cmd_type, "event") == 0) {
        /* Key event: {"cmd": "event", "type": "key", "code": 105, "modifiers": 0} */
        const char *type = json_object_get_string(&cmd, "type");
        if (type && strcmp(type, "key") == 0) {
            int code = json_object_get_int(&cmd, "code", 0);
            int mods = json_object_get_int(&cmd, "modifiers", 0);

            event.type = EVENT_KEY;
            event.data.key.keycode = code;
            event.data.key.modifiers = (uint8_t)mods;
            web_host_queue_event(data, &event);
        }
    } else if (strcmp(cmd_type, "resize") == 0) {
        /* Resize event: {"cmd": "resize", "rows": 24, "cols": 80} */
        int rows = json_object_get_int(&cmd, "rows", 24);
        int cols = json_object_get_int(&cmd, "cols", 80);

        event.type = EVENT_RESIZE;
        event.data.resize.rows = rows;
        event.data.resize.cols = cols;
        web_host_queue_event(data, &event);
    } else if (strcmp(cmd_type, "snapshot") == 0) {
        /* Snapshot request - mark for render */
        data->needs_render = 1;
    } else if (strcmp(cmd_type, "quit") == 0) {
        /* Quit event */
        event.type = EVENT_QUIT;
        web_host_queue_event(data, &event);
    } else if (strcmp(cmd_type, "load") == 0) {
        /* Load file via WebSocket */
        const char *filename = json_object_get_string(&cmd, "filename");
        if (filename) {
            FILE *f = fopen(filename, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);

                char *content = malloc(size + 1);
                if (content) {
                    size_t read = fread(content, 1, size, f);
                    content[read] = '\0';

                    /* Build response */
                    JsonBuilder jb;
                    json_builder_init(&jb);
                    json_object_start(&jb);
                    json_kv_string(&jb, "type", "file");
                    json_kv_string(&jb, "filename", filename);
                    json_kv_string(&jb, "content", content);
                    json_object_end(&jb);

                    mg_ws_send(c, json_builder_get(&jb), strlen(json_builder_get(&jb)),
                              WEBSOCKET_OP_TEXT);
                    json_builder_free(&jb);
                    free(content);
                }
                fclose(f);
            } else {
                /* Send error */
                mg_ws_send(c, "{\"type\":\"error\",\"text\":\"File not found\"}",
                          40, WEBSOCKET_OP_TEXT);
            }
        }
    }

    json_value_free(&cmd);
}

/* Send ViewModel snapshot to WebSocket client */
static void web_host_send_snapshot(WebHostData *data) {
    if (!data->ws_conn || !data->session) return;

    EditorViewModel *vm = editor_session_snapshot(data->session);
    if (!vm) return;

    char *vm_json = jsonrpc_serialize_viewmodel(vm);
    editor_viewmodel_free(vm);

    if (!vm_json) return;

    /* Build response: {"type": "update", "viewmodel": ...} */
    size_t vm_len = strlen(vm_json);
    size_t total_len = vm_len + 32; /* Extra for wrapper */
    char *response = malloc(total_len);
    if (response) {
        snprintf(response, total_len, "{\"type\":\"update\",\"viewmodel\":%s}", vm_json);
        mg_ws_send(data->ws_conn, response, strlen(response), WEBSOCKET_OP_TEXT);
        free(response);
    }

    free(vm_json);
}

/* ======================= Mongoose Event Handler ============================ */

static void web_host_handler(struct mg_connection *c, int ev, void *ev_data) {
    WebHostData *data = (WebHostData *)c->fn_data;

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        /* WebSocket upgrade */
        if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
            mg_ws_upgrade(c, hm, NULL);
            data->ws_conn = c;
            data->needs_render = 1; /* Send initial state */
            return;
        }

        /* REST API endpoints */
        if (mg_match(hm->uri, mg_str("/api/run"), NULL) &&
            mg_strcmp(hm->method, mg_str("POST")) == 0) {
            handle_api_run(c, hm, data);
            return;
        }

        if (mg_match(hm->uri, mg_str("/api/repl"), NULL) &&
            mg_strcmp(hm->method, mg_str("POST")) == 0) {
            handle_api_repl(c, hm, data);
            return;
        }

        if (mg_match(hm->uri, mg_str("/api/save"), NULL) &&
            mg_strcmp(hm->method, mg_str("POST")) == 0) {
            handle_api_save(c, hm, data);
            return;
        }

        if (mg_match(hm->uri, mg_str("/api/load"), NULL) &&
            mg_strcmp(hm->method, mg_str("POST")) == 0) {
            handle_api_load(c, hm, data);
            return;
        }

        /* Serve static files from web_root */
        if (data->web_root) {
            struct mg_http_serve_opts opts = {
                .root_dir = data->web_root,
                .ssi_pattern = NULL
            };
            mg_http_serve_dir(c, hm, &opts);
        } else if (mg_match(hm->uri, mg_str("/"), NULL)) {
            /* Serve embedded HTML for root path if no web_root */
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                         "%s", EMBEDDED_HTML);
        } else {
            mg_http_reply(c, 404, "", "Not found\n");
        }

    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        web_host_process_message(data, c, wm->data.buf, wm->data.len);

    } else if (ev == MG_EV_CLOSE) {
        if (c == data->ws_conn) {
            data->ws_conn = NULL;
        }
    }
}

/* ======================= EditorHost Interface ============================== */

static int web_host_read_event(EditorHost *host, EditorEvent *event, int timeout_ms) {
    WebHostData *data = (WebHostData *)host->data;

    /* Poll mongoose for network events */
    mg_mgr_poll(&data->mgr, timeout_ms > 0 ? timeout_ms : WEB_HOST_POLL_MS);

    /* Check if WebSocket delivered an event */
    if (web_host_dequeue_event(data, event) == 0) {
        /* Mark for render after event is processed */
        data->needs_render = 1;
        return 0; /* Got event */
    }

    return 1; /* Timeout */
}

static void web_host_render(EditorHost *host, EditorSession *session) {
    WebHostData *data = (WebHostData *)host->data;

    /* Store session reference for snapshot requests */
    data->session = session;

    /* Send snapshot only when flagged (after event processed) */
    if (data->needs_render && data->ws_conn) {
        web_host_send_snapshot(data);
        data->needs_render = 0;
    }
}

static int web_host_should_continue(EditorHost *host) {
    WebHostData *data = (WebHostData *)host->data;
    return data->running;
}

static void web_host_destroy(EditorHost *host) {
    if (!host) return;

    WebHostData *data = (WebHostData *)host->data;
    if (data) {
        data->running = 0;
        mg_mgr_free(&data->mgr);
        pthread_mutex_destroy(&data->queue_mutex);
        free(data->web_root);
        free(data);
    }
    free(host);
}

/* ======================= Public API ======================================== */

EditorHost *editor_host_web_create(int port, const char *web_root) {
    EditorHost *host = calloc(1, sizeof(EditorHost));
    if (!host) return NULL;

    WebHostData *data = calloc(1, sizeof(WebHostData));
    if (!data) {
        free(host);
        return NULL;
    }

    /* Initialize mutex */
    if (pthread_mutex_init(&data->queue_mutex, NULL) != 0) {
        free(data);
        free(host);
        return NULL;
    }

    data->port = port > 0 ? port : WEB_HOST_DEFAULT_PORT;
    data->running = 1;

    if (web_root) {
        data->web_root = strdup(web_root);
        if (!data->web_root) {
            pthread_mutex_destroy(&data->queue_mutex);
            free(data);
            free(host);
            return NULL;
        }
    }

    /* Initialize mongoose */
    mg_mgr_init(&data->mgr);

    /* Build listen URL */
    char url[64];
    snprintf(url, sizeof(url), "http://0.0.0.0:%d", data->port);

    /* Start HTTP listener */
    data->listener = mg_http_listen(&data->mgr, url, web_host_handler, data);
    if (!data->listener) {
        fprintf(stderr, "Error: Failed to bind to port %d\n", data->port);
        mg_mgr_free(&data->mgr);
        pthread_mutex_destroy(&data->queue_mutex);
        free(data->web_root);
        free(data);
        free(host);
        return NULL;
    }

    /* Set up host interface */
    host->read_event = web_host_read_event;
    host->render = web_host_render;
    host->should_continue = web_host_should_continue;
    host->destroy = web_host_destroy;
    host->data = data;

    return host;
}

int editor_host_web_get_port(EditorHost *host) {
    if (!host || !host->data) return -1;
    WebHostData *data = (WebHostData *)host->data;
    return data->port;
}

int editor_host_web_run(int port, const char *web_root, const EditorConfig *config) {
    /* Register all languages before creating session */
    loki_lang_init();

    EditorHost *host = editor_host_web_create(port, web_root);
    if (!host) {
        return 1;
    }

    WebHostData *data = (WebHostData *)host->data;
    fprintf(stderr, "Web editor running at http://localhost:%d\n", data->port);
    if (data->web_root) {
        fprintf(stderr, "Serving static files from: %s\n", data->web_root);
    }
    fprintf(stderr, "Press Ctrl-C to stop\n");
    fflush(stderr);

    int result = editor_host_run(host, config);

    host->destroy(host);
    return result;
}

#endif /* LOKI_WEB_HOST */
