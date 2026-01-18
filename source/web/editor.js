/* psnd web editor client - Hybrid CodeMirror + REPL
 *
 * Provides:
 * - CodeMirror-based code editor
 * - jQuery Terminal REPL pane
 * - WebSocket for real-time updates
 * - REST API for code execution
 */

(function() {
  'use strict';

  // UI Elements
  const statusEl = document.getElementById('status');
  const modeEl = document.getElementById('mode');
  const filenameEl = document.getElementById('filename');
  const dirtyEl = document.getElementById('dirty');
  const langEl = document.getElementById('lang');
  const messageEl = document.getElementById('message');
  const positionEl = document.getElementById('position');

  // Initialize CodeMirror
  const editor = CodeMirror.fromTextArea(document.getElementById('editor'), {
    lineNumbers: true,
    theme: 'monokai',
    mode: 'javascript',  // Default mode, will be updated based on file type
    matchBrackets: true,
    autoCloseBrackets: true,
    indentUnit: 2,
    tabSize: 2,
    indentWithTabs: false,
    lineWrapping: false,
    extraKeys: {
      'Ctrl-Enter': runCode,
      'Cmd-Enter': runCode,
      'Ctrl-S': saveFile,
      'Cmd-S': saveFile
    }
  });

  // Track editor state
  let currentFilename = null;
  let isDirty = false;
  let detectedLang = 'text';

  // WebSocket connection
  let ws = null;
  let reconnectTimeout = null;

  // jQuery Terminal instance
  let term = null;

  // Initialize terminal
  function initTerminal() {
    term = $('#terminal').terminal(function(command, terminal) {
      if (command.trim() === '') {
        return;
      }
      // Send REPL command
      sendReplCommand(command);
    }, {
      greetings: 'psnd REPL - Type code to evaluate, or :help for commands',
      prompt: '> ',
      name: 'psnd',
      height: '100%',
      completion: false,
      outputLimit: 1000,
      onInit: function() {
        // Terminal ready
      }
    });
  }

  // Status display
  function setStatus(text, state) {
    statusEl.textContent = text;
    statusEl.className = 'status ' + (state || 'disconnected');
  }

  function showMessage(text, type) {
    messageEl.textContent = text;
    messageEl.className = 'message ' + (type || '');
    // Auto-clear after 5 seconds
    setTimeout(function() {
      if (messageEl.textContent === text) {
        messageEl.textContent = '';
        messageEl.className = 'message';
      }
    }, 5000);
  }

  // WebSocket connection
  function connect() {
    if (ws && ws.readyState === WebSocket.OPEN) return;

    setStatus('Connecting...', 'connecting');

    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(proto + '//' + location.host + '/ws');

    ws.onopen = function() {
      setStatus('Connected', 'connected');
      // Request initial file content if we have a filename
      if (currentFilename) {
        send({ cmd: 'load', filename: currentFilename });
      }
    };

    ws.onclose = function() {
      setStatus('Disconnected', 'disconnected');
      ws = null;
      // Reconnect after delay
      if (!reconnectTimeout) {
        reconnectTimeout = setTimeout(function() {
          reconnectTimeout = null;
          connect();
        }, 2000);
      }
    };

    ws.onerror = function() {
      setStatus('Error', 'disconnected');
    };

    ws.onmessage = function(e) {
      try {
        const msg = JSON.parse(e.data);
        handleMessage(msg);
      } catch (err) {
        console.error('Failed to parse message:', err);
      }
    };
  }

  function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
    }
  }

  // Handle incoming messages
  function handleMessage(msg) {
    if (msg.type === 'file') {
      // File content loaded
      editor.setValue(msg.content || '');
      currentFilename = msg.filename;
      filenameEl.textContent = msg.filename || '[No Name]';
      isDirty = false;
      dirtyEl.textContent = '';
      updateLang(msg.filename);
    } else if (msg.type === 'output') {
      // REPL output
      if (term) {
        term.echo(msg.text, { raw: msg.raw || false });
      }
    } else if (msg.type === 'error') {
      // Error message
      if (term) {
        term.error(msg.text);
      }
      showMessage(msg.text, 'error');
    } else if (msg.type === 'result') {
      // Code execution result
      if (term) {
        if (msg.ok) {
          if (msg.output) {
            term.echo(msg.output);
          }
          term.echo('[[;#4ec9b0;]OK]');
        } else {
          term.error(msg.error || 'Execution failed');
        }
      }
    } else if (msg.type === 'saved') {
      // File saved
      isDirty = false;
      dirtyEl.textContent = '';
      showMessage('Saved ' + msg.filename, 'success');
    } else if (msg.type === 'status') {
      // Status update
      if (msg.mode) {
        modeEl.textContent = msg.mode;
      }
      if (msg.lang) {
        langEl.textContent = msg.lang;
      }
    }
  }

  // Detect language from filename
  function updateLang(filename) {
    if (!filename) {
      detectedLang = 'text';
      langEl.textContent = '';
      editor.setOption('mode', 'text');
      return;
    }

    const ext = filename.split('.').pop().toLowerCase();
    let mode = 'text';
    let lang = ext.toUpperCase();

    switch (ext) {
      case 'alda':
        mode = 'text';
        lang = 'ALDA';
        break;
      case 'joy':
        mode = 'text';
        lang = 'JOY';
        break;
      case 'js':
        mode = 'javascript';
        lang = 'JS';
        break;
      case 'lua':
        mode = 'lua';
        lang = 'LUA';
        break;
      case 'c':
      case 'h':
        mode = 'text/x-csrc';
        lang = 'C';
        break;
      case 'py':
        mode = 'python';
        lang = 'PY';
        break;
      default:
        mode = 'text';
    }

    detectedLang = lang.toLowerCase();
    langEl.textContent = lang;
    editor.setOption('mode', mode);
  }

  // Run code
  function runCode() {
    const code = editor.getSelection() || editor.getValue();
    if (!code.trim()) {
      showMessage('No code to run', 'error');
      return;
    }

    // Show in terminal what we're running
    if (term) {
      term.echo('[[;#569cd6;]>>> Running code...]');
    }

    // Send via REST API
    fetch('/api/run', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        code: code,
        lang: detectedLang
      })
    })
    .then(function(response) { return response.json(); })
    .then(function(result) {
      handleMessage({ type: 'result', ok: result.ok, output: result.output, error: result.error });
    })
    .catch(function(err) {
      handleMessage({ type: 'error', text: 'Request failed: ' + err.message });
    });
  }

  // Send REPL command
  function sendReplCommand(command) {
    fetch('/api/repl', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        command: command,
        lang: detectedLang
      })
    })
    .then(function(response) { return response.json(); })
    .then(function(result) {
      if (result.ok && result.output) {
        term.echo(result.output);
      } else if (!result.ok) {
        term.error(result.error || 'Command failed');
      }
    })
    .catch(function(err) {
      term.error('Request failed: ' + err.message);
    });
  }

  // Save file
  function saveFile() {
    const content = editor.getValue();
    const filename = currentFilename || prompt('Enter filename:');
    if (!filename) return;

    fetch('/api/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        filename: filename,
        content: content
      })
    })
    .then(function(response) { return response.json(); })
    .then(function(result) {
      if (result.ok) {
        currentFilename = filename;
        handleMessage({ type: 'saved', filename: filename });
        filenameEl.textContent = filename;
      } else {
        handleMessage({ type: 'error', text: result.error || 'Save failed' });
      }
    })
    .catch(function(err) {
      handleMessage({ type: 'error', text: 'Save failed: ' + err.message });
    });

    return false; // Prevent default
  }

  // Update cursor position display
  editor.on('cursorActivity', function() {
    const cursor = editor.getCursor();
    positionEl.textContent = 'Ln ' + (cursor.line + 1) + ', Col ' + (cursor.ch + 1);
  });

  // Track dirty state
  editor.on('change', function() {
    if (!isDirty) {
      isDirty = true;
      dirtyEl.textContent = '[+]';
    }
  });

  // Button handlers
  document.getElementById('btn-run').addEventListener('click', runCode);
  document.getElementById('btn-save').addEventListener('click', saveFile);
  document.getElementById('btn-clear').addEventListener('click', function() {
    if (term) {
      term.clear();
    }
  });

  // Resizer functionality
  (function() {
    const resizer = document.getElementById('resizer');
    const editorPanel = document.querySelector('.editor-panel');
    const replPanel = document.querySelector('.repl-panel');
    let isResizing = false;

    resizer.addEventListener('mousedown', function(e) {
      isResizing = true;
      document.body.style.cursor = 'col-resize';
      e.preventDefault();
    });

    document.addEventListener('mousemove', function(e) {
      if (!isResizing) return;

      const containerWidth = document.querySelector('.main').offsetWidth;
      const newEditorWidth = e.clientX;
      const minWidth = 200;

      if (newEditorWidth > minWidth && (containerWidth - newEditorWidth - 4) > minWidth) {
        editorPanel.style.flex = 'none';
        editorPanel.style.width = newEditorWidth + 'px';
        replPanel.style.width = (containerWidth - newEditorWidth - 4) + 'px';
        editor.refresh();
      }
    });

    document.addEventListener('mouseup', function() {
      if (isResizing) {
        isResizing = false;
        document.body.style.cursor = '';
      }
    });
  })();

  // Load file from URL parameter
  function loadFromUrl() {
    const params = new URLSearchParams(window.location.search);
    const file = params.get('file');
    if (file) {
      currentFilename = file;
      filenameEl.textContent = file;
      updateLang(file);
      // Request file content via WebSocket once connected
      if (ws && ws.readyState === WebSocket.OPEN) {
        send({ cmd: 'load', filename: file });
      }
    }
  }

  // Keyboard shortcuts
  document.addEventListener('keydown', function(e) {
    // Prevent Ctrl+S from triggering browser save
    if ((e.ctrlKey || e.metaKey) && e.key === 's') {
      e.preventDefault();
      saveFile();
    }
  });

  // Initialize
  $(document).ready(function() {
    initTerminal();
    connect();
    loadFromUrl();
    editor.focus();
  });

})();
