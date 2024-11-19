/*
 * MIT License
 * Copyright (c) 2023 Benoit Baudaux
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <emscripten.h>

#include <cJSON.h>

enum event {

  TERMINAL_BUTTON_CLICKED = 1,
  SUGGESTION_CLICKED,
  APP_CLICKED,
  CONNECT_CLICKED,
  REGISTER_CLICKED,
  LOGIN_UPDATED,
  START_CMD,
};

static bool infoIFrameShown = false;
static cJSON * root = NULL;
static cJSON * apps = NULL;
static int nb_apps = 0;

static pid_t login_pid = -1;

EM_JS(int, wait_event, (), {
    
    return Asyncify.handleSleep(function (wakeUp) {

	Module.wakeUp = wakeUp;
      });
  }
);

void sigchld_handler(int sig) {
  
  int status;
  
  int pid = wait(&status);
}

void start_terminal() {

  chdir("/home");

  int pid = fork();

  if (pid == 0) {

    execl ("/usr/bin/havoc", "/usr/bin/havoc", (void*)0);
  }
}

int start_app(const char * cmd, const char * arg) {

  pid_t pid = fork();
  
  if (pid == 0) { // Child process

    if (!arg) {
      execl (cmd, cmd, (void*)0);
    }
    else {

      char * p;

      if ((p=strchr(arg, ' ')) != NULL) {

	char * argv[128];
	char * args;
	int i = 0;
	int j;

	argv[0] = cmd;

	args = malloc(strlen(arg+1));
	strcpy(args, arg);

	j = 1;
	
	argv[j] = args;
	
	i += p-arg;
	args[i] = 0;

	++j;

	argv[j] = args+i+1;

	while ((p=strchr(arg+i+1, ' ')) != NULL) {

	  i += p-(arg+i+1);
	  args[i] = 0;

	  ++j;

	  argv[j] = args+i+1;
	}

	++j;
	argv[j] = NULL;

	execv (cmd, argv);

	free(args);
      }
      else {
	execl (cmd, cmd, arg, (void*)0);
      }
    }
  }

  return pid;
}

int read_login_file(char * username) {

  FILE * token_file = fopen("/home/.login", "r");

  if (!token_file) {
    
    return -1;
  }

  char str[256];

  int size = fread(str, 1, 256, token_file);
  fclose(token_file);

  str[size] = 0;

  char * e = strchr(str, '\n');

  if (e) {

    strncpy(username, str+9, e-str-9);

    return 0;
  }

  return -1;
}

void read_login() {

  char username[128];

  if (!read_login_file(username)) {

    EM_ASM({

	console.log("read_login: user="+$0);

	let connect = document.getElementById("connect");
	
	connect.innerHTML = UTF8ToString($0);

	let register = document.getElementById("register");

	register.classList.add("hidden");

      }, username);
  }
  else {

    EM_ASM({

	console.log("read_login: no user");

	let connect = document.getElementById("connect");
	
	connect.innerHTML = "Connect";

	let register = document.getElementById("register");

	register.classList.remove("hidden");

      });
  }
}

void update_login() {

  EM_ASM({

      let connect = document.getElementById("connect");
      let register = document.getElementById("register");
      
      if (Module.username == "connect") {

	connect.innerHTML = "Connect";
	
	register.classList.remove("hidden");
      }
      else {

	connect.innerHTML = Module.username;

	register.classList.add("hidden");
      }

    });
}

int main(int argc, char * argv[]) {

  signal(SIGCHLD, sigchld_handler);
  
  setenv("HOME", "/home", 0);
  setenv("PWD", "/home", 0);
  setenv("USER", "root", 0);
  setenv("LOGNAME", "root", 0);

  chdir("/home");
  
  EM_ASM({

      var all_apps = [];
      var fuse;
      
      function get_apps() {

	window.fetch("/get_all_apps.php")
	    .then((response) => {
		
		if (!response.ok) {

		  return { apps: [

		    {
	    "pkg": "Vim",
	    "description": "Highly customizable text editor",
	    "username": "system",
	    "startcmd": "\/usr\/bin\/vim",
	    "category": "Text editor",
	    
	},
	{
	    "pkg": "GNU ed",
	    "description": "Line text editor for Unix-like OS",
	    "username": "system",
	    "startcmd": "\/usr\/bin\/ed",
	    "category": "Text editor",
	    "source": "https:\/\/github.com\/happy5214\/gnu-ed",
	    "icon": "\/netfs\/usr\/local\/share\/gnuchess\/Official_gnu.svg"
	} 

                  ]};
		}

		return response.json();
	      })
	    .then((response) => {
		
		//console.log(response);

		if ('apps' in response) {

		  all_apps = response.apps;

		  var options = {
		  shouldSort: true,
		  tokenize: true,
		  matchAllTokens: true,
		  includeScore: true,
		  threshold: 0.3,
		  location: 0,
		  distance: 100,
		  maxPatternLength: 32,
		  minMatchCharLength: 1,
		  keys: ['pkg', 'category','description', 'username', 'startcmd']
		  };

		  fuse = new Fuse(all_apps, options);
		}
	      });
      }
      
      function handleMouseDown (x, y) {

	let minimize = 0;

	//console.log("Oxygen: mouse down");

	let axel = document.getElementById("axel");
	let axel_rect = axel.getBoundingClientRect();
	let term = document.getElementById("term");
	let term_rect = term.getBoundingClientRect();
	let input_body = document.getElementById("input_body");
	let input_rect = input_body.getBoundingClientRect();
	

	if ( (x >= axel_rect.left) && (x <= axel_rect.right) && (y >= axel_rect.top) && (y <= axel_rect.bottom) ) {

	  minimize = 1;
	  show_all_apps();
	}
	else if ( (x >= term_rect.left) && (x <= term_rect.right) && (y >= term_rect.top) && (y <= term_rect.bottom) ) {

	  minimize = 1;
	  Module.wakeUp(1); // TERMINAL_BUTTON_CLICKED
	}
	else if ( (x >= input_rect.left) && (x <= input_rect.right) && (y >= input_rect.top) && (y <= input_rect.bottom) ) {

	  minimize = 1;

	  setTimeout(() => {

	      input_body.focus();
	    }, 1);
	}
	

	if (minimize) {

	  let m = new Object();
		
	  m.type = 13; // minimise all windows
	  m.pid = Module.getpid() & 0x0000ffff;
		    
	  window.parent.postMessage(m);
	}

	return 0;
      }

      function desktop_foreground() {

	let m = new Object();
		
	m.type = 17; // desktop on top
	m.pid = Module.getpid() & 0x0000ffff;
		    
	window.parent.postMessage(m);
      }

      function show_apps(apps) {

	console.log(apps);

	desktop_foreground();
	
	const find_apps_body = document.getElementById("find_apps_body");

	find_apps_body.classList.add("hidden");

	const find_apps_header = document.getElementById("find_apps_header");

	find_apps_header.classList.remove("hidden");

	const header = document.getElementById("header");

	const list_apps = document.getElementById("list_apps");

	list_apps.classList.remove("hidden");
	list_apps.style.top = header.getBoundingClientRect().bottom + "px";
	list_apps.style.height = (window.innerHeight-header.getBoundingClientRect().height) + "px";

	while (list_apps.firstChild)
	  list_apps.removeChild(list_apps.firstChild);

	for (let app of apps) {

	  let item = document.createElement("li");

	  let name = document.createElement("span");

	  name.innerText = ('item' in app)?app.item.pkg:app.pkg;

	  item.appendChild(name);

	  let cat = (('item' in app)?app.item.category:app.category);

	  if (cat.length > 0) {
	    
	    let category = document.createElement("span");
	    category.innerText = "("+ cat +")";
	    item.appendChild(category);
	  }

	  item.path = ('item' in app)?app.item.startcmd:app.startcmd;
	  item.username = ('item' in app)?app.item.username:app.username;
	  item.pkg = ('item' in app)?app.item.pkg:app.pkg;

	  item.onclick = (event) => {

	    event.stopPropagation();

	    if (event.srcElement.tagName == "SPAN")
	      window.eventSource = event.srcElement.parentElement;
	    else
	      window.eventSource = event.srcElement;

	    Module.wakeUp(3); // APP_CLICKED
	  };

	  list_apps.appendChild(item);
	}

	if (apps.length == 0) {

	  let item = document.createElement("li");

	  item.innerHTML = "sorry, no result";

	  item.onclick = (event) => {

	    event.stopPropagation();
	  };
	  
	  list_apps.appendChild(item);
	}
	
      }

      function show_suggestions(result) {

	let show_all = document.getElementById("show_all");

	show_all.classList.add("hidden");

	let sugg = document.getElementById("suggestions");

	sugg.classList.remove("hidden");
	
	while (sugg.firstChild)
	  sugg.removeChild(sugg.firstChild);

	for (let i=0; (i < result.length) && (i < 4); i += 1) {

	  let li = document.createElement("li");

	  li.innerHTML = result[i].item.pkg;
	  li.path = result[i].item.startcmd;
	  li.username = result[i].item.username;
	  li.pkg = result[i].item.pkg;

	  li.onclick = (event) => {

	    event.stopPropagation();

	    window.eventSource = event.srcElement;

	    Module.wakeUp(2); // SUGGESTION_CLICKED
	  };

	  sugg.appendChild(li);
	}

	if (result.length > 4) {

	  let li = document.createElement("li");

	  li.innerHTML = "and more ...";

	  li.onclick = (function(res) {

	    return (event) => {

	      event.stopPropagation();

	      let inputBody = document.getElementById("input_body");
	      let inputHeader = document.getElementById("input_header");

	      inputHeader.value = inputBody.value;

	      setTimeout(() => {

		  inputHeader.focus();
		}, 1);
	    
	      show_apps(res);
	    };
	    })(result);

	  sugg.appendChild(li);
	}
	else if (result.length == 0) {

	  let li = document.createElement("li");

	  li.innerHTML = "sorry, no result";

	  sugg.appendChild(li);
	}
	
      }

      function hide_suggestions() {

	let show_all = document.getElementById("show_all");

	show_all.classList.remove("hidden");

	let sugg = document.getElementById("suggestions");

	sugg.classList.add("hidden");

	while (sugg.firstChild)
	  sugg.removeChild(sugg.firstChild);
      }

      function hide_all_apps() {

	let show_all = document.getElementById("show_all");

	show_all.classList.remove("hidden");

	let sugg = document.getElementById("suggestions");

	sugg.classList.add("hidden");

	const find_apps_body = document.getElementById("find_apps_body");

	find_apps_body.classList.remove("hidden");

	const find_apps_header = document.getElementById("find_apps_header");

	find_apps_header.classList.add("hidden");

	const list_apps = document.getElementById("list_apps");

	list_apps.classList.add("hidden");
      }

      Module.hide_all_apps = hide_all_apps;

      function show_all_apps() {

	let inputHeader = document.getElementById("input_header");

	inputHeader.value = inputBody.value;

	setTimeout(() => {

	    inputHeader.focus();
	  }, 1);

	if (inputBody.value.length > 0) {

	  let res = fuse.search(inputBody.value);

	  show_apps(res);
	}
	else {

	  show_apps(all_apps);
	}
      }

      get_apps();

      let term = document.getElementById("term");
      
      term.addEventListener("click", (event) => {

	  event.stopPropagation();

	  Module.wakeUp(1); // TERMINAL_BUTTON_CLICKED
	});

      document.body.addEventListener("click", (event) => {

	  //console.log("Oxygen mouse down: click outside window !");
	  console.log(event);

	  hide_all_apps();

	  let m = new Object();
		
	  m.type = 8; //  search clicked window
	  m.pid = Module.getpid() & 0x0000ffff;
	  m.x = event.clientX;
	  m.y = event.clientY;
		    
	  window.parent.postMessage(m);
	      
	}, false);

      window.addEventListener('message', (event) => {

	  //console.log(event.data);

	  if (event.data.type == 8) { // mouse down

	    if (!handleMouseDown(event.data.x, event.data.y)) {

	      let m = new Object();
		
	      m.type = 9; // continue searching clicked window
	      m.pid = Module.getpid() & 0x0000ffff;
	      m.x = event.data.x;
	      m.y = event.data.y;
		    
	      window.parent.postMessage(m);
	    }
	  }
	});
      
      if (!Module.iframeShown) {

	    Module.iframeShown = true;

	    let m = new Object();
	
	    m.type = 7; // show iframe and hide body
	    m.pid = Module.getpid() & 0x0000ffff;
	    
	    window.parent.postMessage(m);
      }

      let inputBody = document.getElementById("input_body");

      inputBody.addEventListener("keyup", (event) => {

	  if (event.key == 'Enter') {

	    let sugg = document.getElementById("suggestions");

	    if (sugg.children && (sugg.children.length == 1)) {

	      window.eventSource = sugg.firstChild;

	      Module.wakeUp(2);
	    }
	    else {

	      show_all_apps();
	    }
	  }
	  else if (inputBody.value.length > 0) {

	    let res = fuse.search(inputBody.value);

	    show_suggestions(res);
	  }
	  else {

	    hide_suggestions();
	  }
	});

      let inputHeader = document.getElementById("input_header");

      inputHeader.addEventListener("keyup", (event) => {

	  if (inputHeader.value.length > 0) {

	    let res = fuse.search(inputHeader.value);

	    show_apps(res);
	  }
	  else {

	    show_apps(all_apps);
	  }
	  
	});

      inputHeader.addEventListener("click", (event) => {

	  event.stopPropagation();
	});

      let show_all = document.getElementById("show_all");
      
      show_all.addEventListener("click", (event) => {

	  event.stopPropagation();
	  
	  show_apps(all_apps);
	  
	});

      let axel = document.getElementById("axel");

      axel.addEventListener("click", (event) => {

	  event.stopPropagation();

	  const input_header = document.getElementById("input_header");

	  if (find_apps_header.classList.contains("hidden")) {

	    show_all_apps();
	  }
	  else {
	    
	    hide_all_apps();
	  }
	  
	});

      let connect = document.getElementById("connect");

      connect.addEventListener("click", (event) => {

	  event.stopPropagation();

	  Module.wakeUp(4); // CONNECT_CLICKED
	});

      let register = document.getElementById("register");

      register.addEventListener("click", (event) => {

	  event.stopPropagation();

	  Module.wakeUp(5); // RESGISTER_CLICKED
	});

      let bc = new BroadcastChannel('channel.desktop');

      bc.onmessage = (messageEvent) => {

	if ('user' in messageEvent.data) {
	  
	  Module.username = messageEvent.data.user;

	  Module.wakeUp(6); // LOGIN_UPDATED
	}
	else if ('start' in messageEvent.data) {

	  Module.start_cmd = messageEvent.data.start;
	  Module.path = messageEvent.data.path;
	  Module.wakeUp(7); // START_CMD
	}
      };
	  
    });

  read_login();
  
  while (1) {

    int ret = wait_event();

    switch(ret) {

      case TERMINAL_BUTTON_CLICKED:

	  start_terminal();
      
        break;

      case SUGGESTION_CLICKED:
      case APP_CLICKED:
	{

	  char path[1024];
	  char username[128];
	  char pkg[1024];

	  EM_ASM({

	      stringToUTF8(window.eventSource.path, $0, 1024);
	      stringToUTF8(window.eventSource.username, $1, 128);
	      stringToUTF8(window.eventSource.pkg, $2, 1024);

	      Module.hide_all_apps();

	    }, path, username, pkg);

	  if (strcmp(path, "/usr/bin/havoc") == 0) {

	    chdir("/home");

	    start_app("/usr/bin/havoc", NULL);
	  }
	  else if (strcmp(path, "/usr/bin/exastore") == 0) {

	    chdir("/home");

	    start_app("/usr/bin/exastore", NULL);
	  }
	  else {

	    if (strcmp(username, "system")) {

	      char dir[1024];

	      sprintf(dir, "/usr/store/%s/%s", username, pkg);
	      chdir(dir);
	    }
	    else {

	      chdir("/home");
	    }
	    
	    start_app("/usr/bin/havoc", path);
	  }
      
        break;
	}

      case CONNECT_CLICKED:
	{
	
	  login_pid = start_app("/usr/bin/exalogin", NULL);
	
	  break;
	}

      case REGISTER_CLICKED:
	
	start_app("/usr/bin/exasignup", NULL);
	break;

      case LOGIN_UPDATED:

	update_login();
	break;

      case START_CMD:
	{
	  char cmd[256];
	  char path[1024];

	  EM_ASM({

	      console.log(Module.start_cmd);
	      console.log(Module.path);

	      stringToUTF8(Module.start_cmd, $0, 256);
	      stringToUTF8(Module.path, $1, 1024);

	    }, cmd, path);

	  // Set cwd to app directory
	
	  chdir(path);
	
	  start_app("/usr/bin/havoc", cmd);

	  break;
	}

      default:
        break;
    }
  }

  return 0;
}

