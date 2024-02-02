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
#include <sys/wait.h>

#include <emscripten.h>

#include <cJSON.h>

#define NB_CHILDREN_MAX 128

enum event {

  TERMINAL_BUTTON_CLICKED = 1,
  SEARCH_INPUT_CLICKED,
  SEARCH_INPUT_CHANGED,
  ENTER_KEY_PRESSED,
  SEARCH_RESULT_SELECTED = 100
};

static bool infoIFrameShown = false;
static cJSON * root = NULL;
static cJSON * apps = NULL;
static int nb_apps = 0;

void open_info() {

  return;

  infoIFrameShown = true;

  EM_ASM({
      
      let iframe = document.createElement("iframe");

      iframe.id = "info";
      iframe.src = "/netfs/usr/local/share/info/index.html";
  
      document.body.appendChild(iframe);
    });
}

void close_info() {

  return;

  infoIFrameShown = false;

  EM_ASM({
      
      let iframe = document.getElementById("info");
  
      iframe.style.display = "none";
    });
}

EM_JS(int, wait_event, (), {
    
    return Asyncify.handleSleep(function (wakeUp) {

	Module.wakeUp = wakeUp;
      });
  }
  );

int start_app(const char * cmd, const char * arg) {

  pid_t pid = fork();
  
  if (pid == 0) { // Child process

    execl (cmd, cmd, arg, (void*)0);
  }

  return pid;
}

void clearSearchResult() {

  EM_ASM({

      let lists = document.getElementsByTagName("ul");

      if (!lists || (lists.length == 0)) {

	let list = document.createElement("ul");
	document.body.appendChild(list);
      }
      else {

	while (lists[0].firstChild) {

	  lists[0].removeChild(lists[0].firstChild);
	}
      }

      lists[0].style.visibility = "hidden";

      lists[0].itemIndex = 0;
    });
}

void addSearchResult(const char * name, const char * desc, const char * category, const char * source, const char * icon, const char * path, const char * mode) {

  EM_ASM({

      let list = document.getElementsByTagName("ul")[0];

      list.style.visibility = "visible";

      let item = document.createElement("li");
      item.index = list.itemIndex;

      list.itemIndex += 1;

      let logo = document.createElement("div");

      logo.style.backgroundImage = "url("+UTF8ToString($4)+")";
      logo.style.backgroundSize = "contain";
      logo.style.backgroundRepeat = "no-repeat";
      logo.style.backgroundPosition = "bottom";
      logo.style.width = "20px";
      logo.style.height = "20px";
      logo.style.display = "inline-block";

      item.appendChild(logo);

      let name = document.createElement("span");

      name.innerText = UTF8ToString($0);

      item.appendChild(name);

      let desc = document.createElement("span");

      desc.innerText = UTF8ToString($1);

      item.appendChild(desc);

      let category = document.createElement("span");

      category.innerText = UTF8ToString($2);

      item.appendChild(category);

      const link = UTF8ToString($3);
      
      if (link.length > 0) {

	let source = document.createElement("a");

	source.setAttribute("target", "_blank");

	source.innerText = "Source";
      
	source.href = link;

	item.appendChild(source);

	source.addEventListener("mousedown", (event) => {
	  
	    event.stopPropagation();
	  
	});
      }

      item.addEventListener("mousedown", (event) => {
	  
	  //console.log(event);

	  let index = event.target.index || event.target.parentElement.index;

	  Module.wakeUp(100+index); // SEARCH_RESULT_SELECTED
	  
	});

      item.path = UTF8ToString($5);

      if ($4) {

	item.mode = UTF8ToString($6);
      }

      list.appendChild(item);
      
    }, name, desc, category, source, icon, path, mode);
}

void show_all_apps() {

  clearSearchResult();

  for (int i = 0; i < nb_apps; ++i) {

    cJSON * app = cJSON_GetArrayItem(apps, i);

    cJSON * app_name = cJSON_GetObjectItem(app, "name");
    cJSON * app_desc = cJSON_GetObjectItem(app, "desc");
    cJSON * app_category = cJSON_GetObjectItem(app, "category");
    cJSON * app_source = cJSON_GetObjectItem(app, "source");
    cJSON * app_icon = cJSON_GetObjectItem(app, "icon");
    cJSON * app_path = cJSON_GetObjectItem(app, "path");
    cJSON * app_mode = cJSON_GetObjectItem(app, "mode");

    addSearchResult(cJSON_GetStringValue(app_name), cJSON_GetStringValue(app_desc), cJSON_GetStringValue(app_category), cJSON_GetStringValue(app_source), cJSON_GetStringValue(app_icon), cJSON_GetStringValue(app_path), (app_mode != NULL)?cJSON_GetStringValue(app_mode):NULL);
  }
}

void start_about() {

  int pid = fork();

  if (pid == 0) {

    execl ("/usr/bin/about", "/usr/bin/about", (void*)0);
  }
}

void sigchld_handler(int sig) {
  
  /*EM_ASM({

      console.log("Oxygen: sigchld_handler "+$0);
      
      }, sig);*/

  int status;
  
  int pid = wait(&status);

  /*EM_ASM({

      console.log("Oxygen: end of process "+$0);
      
      }, pid);*/
}

int main(int argc, char * argv[]) {

  signal(SIGCHLD, sigchld_handler);
  
  setenv("HOME", "/home", 0);
  setenv("PWD", "/home", 0);
  setenv("USER", "root", 0);
  setenv("LOGNAME", "root", 0);

  chdir("/home");

  /*background-image: url("/netfs/usr/share/background.png");
	  background-size: contain;
	  background-repeat: no-repeat;
	  background-position: center;

  let copyright = document.createElement("span");
      copyright.id = "copyright";
      copyright.innerHTML = "&copy;Benoit Baudaux";
      document.body.appendChild(copyright);*/
  
  EM_ASM({

      let styles = `

	body {
	
          overflow: hidden;
	  
        }

        \#copyright {

	  position:absolute;
	  top: calc(100% - 20px);
	  left: calc(100% - 120px);
	  color: white;
        }

        \#search_div {

          border: solid 1px white;
          border-radius: 20px;
          width: 300px;
          height: 35px;
          margin: auto;
          display: flex;
          align-items: center;
          padding-left: 10px;
	  box-shadow: 3px 3px 3px rgba(100, 100, 100, 0.4);
        }

        input {

          background: none;
          border: none;
          outline: none;
          color: #101010;
          font-family: sans-serif;
          font-size: 22px;
          margin-left: 10px;
        }

        ::placeholder {

	  color: white;
        }

        ul {

          width: 800px;
          margin-left: auto;
          margin-right: auto;
          background-color: white;
          color: #303030;
          font-family: sans-serif;
          font-size: 20px;
          list-style-type: none;
          padding: 10px;
          border-radius: 10px;
          opacity: 90%;
          cursor: pointer;
	  max-height: calc(100% - 200px);
	  overflow: auto;
        }

        li:hover {

	  background-color: lightgrey;
          cursor: pointer;
        }

        li > span {

	  margin: 10px;
        }

       li > span:nth-child(3) {

	  font-size: 15px;
	  font-style: italic;
	  color: deepskyblue;
	}

        li > span:nth-child(4) {

	  font-size: 15px;
	  color: coral;
	}

        li > span:nth-child(4)::before {

	  content: '(';
	}

        li > span:nth-child(4)::after {

	content: ')';
	}

        li > a:nth-child(5) {

	  font-size: 15px;
	  color: grey;
b	}

        iframe {

	  margin: 20px auto auto auto;
	  display: block;
	  width: calc(100% - 100px);
	  height: calc(90% - 150px);
	  border: none;
	  border-radius: 10px;
	  overflow: scroll;
        }
      `;

      let styleSheet = document.createElement("style");
      styleSheet.innerText = styles;
      document.head.appendChild(styleSheet);

      let randomNumber = 60;

      while ( ( (randomNumber > 55) && (randomNumber < 80) ) || ( (randomNumber > 175) && (randomNumber < 180) ) ) {

	  randomNumber = Math.floor(Math.random() * 360);
      }

      document.body.style.backgroundColor = `hsl(${randomNumber}, 100%, 70%)`;

    /*let term_icon = document.createElement("img");

      term_icon.src = "/netfs/usr/share/term_icon.png";
      term_icon.style.position = "absolute";
      term_icon.style.top = "20px";
      term_icon.style.left = "20px";
      term_icon.style.width = "50px";

      term_icon.setAttribute("title", "Open new terminal window");

      document.body.appendChild(term_icon);*/

      let header = document.createElement("div");

      header.style.height = (100+window.innerHeight/10)+"px";

      document.body.appendChild(header);

      let search_div = document.createElement("div");
      search_div.id = "search_div";
      
      document.body.appendChild(search_div);

      let search_icon = document.createElement("img");
      search_icon.src = "/netfs/usr/share/search_icon.png";
      search_icon.style.width = "20px";
      search_icon.style.height = "20px";
      
      search_div.appendChild(search_icon);

      let search_input = document.createElement("input");

      search_input.setAttribute("placeholder", "Find an app");

      search_input.addEventListener("keydown", (event) => {

	  //console.log(event);

	  if (event.keyCode == 13) {

	    Module.wakeUp(4); // Enter key pressed
	  }
	});

      search_input.addEventListener("keyup", (event) => {

	  //console.log(event);

	  Module.wakeUp(3); // Search input changed
	  
	});

      search_div.appendChild(search_input);

      function handleMouseDown (x, y) {

	//console.log("Oxygen: mouse down");

	    //let term_icon_rect = term_icon.getBoundingClientRect();
	    let search_div_rect = search_div.getBoundingClientRect();

	    //console.log(term_icon_rect);
	    
	    /*if ( (x >= term_icon_rect.left) && (x <= term_icon_rect.right) && (y >= term_icon_rect.top) && (y <= term_icon_rect.bottom) ) {

	      //console.log("Oxygen: mouse down on term icon");

	      Module.wakeUp(1); // Terminal button clicked

	      return 1;
	      
	    }
	    else */if ( (x >= search_div_rect.left) && (x <= search_div_rect.right) && (y >= search_div_rect.top) && (y <= search_div_rect.bottom) ) {

	      //console.log("Oxygen: mouse down on term icon");

	      let m = new Object();
		
	      m.type = 13; // minimise all windows
	      m.pid = Module.getpid() & 0x0000ffff;
		    
	      window.parent.postMessage(m);

	      search_input.focus();
	      
	      Module.wakeUp(2); // Search input clicked

	      return 1;
	    }

	    return 0;
      }

      let logo_div = document.createElement("div");
      logo_div.style.position = "absolute";
      logo_div.style.display = "flex";
      logo_div.style.width = "100vw";
      logo_div.style.justifyContent = "center";
      logo_div.style.paddingTop = "5vh";

      document.body.appendChild(logo_div);

      let logo = document.createElement("img");
      logo.src = "/netfs/usr/share/logo.svg";
      logo.style.width = "40vw";
      logo.style.height = "40vh";
      logo.style.margin = "auto";
      
      logo_div.appendChild(logo);

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

      document.body.addEventListener("mousedown", (event) => {

	  //console.log("Oxygen mouse down: click outside window !");
	  //console.log(event);

	  handleMouseDown(event.clientX, event.clientY);
	      
	}, false);
      
      
      if (!Module.iframeShown) {

	    Module.iframeShown = true;

	    let m = new Object();
	
	    m.type = 7; // show iframe and hide body
	    m.pid = Module.getpid() & 0x0000ffff;

	    window.parent.postMessage(m);
      }

      
    });

//open_info();

  start_about();

  FILE * f = fopen("/usr/share/app_db.json", "r");

  if (f) {

      char * str;

      fseek(f, 0, SEEK_END);
      long size = ftell(f);
      str = (char *)malloc(size);

      fseek(f, 0, SEEK_SET);
      fread(str, 1, size, f);
      
      fclose(f);
      
      //printf("app_db.json: \n%s\n", str);

      root = cJSON_Parse(str);

      if (root) {
	
	//printf("%s\n", cJSON_Print(root));

	apps = cJSON_GetObjectItem(root, "apps");

	nb_apps = cJSON_GetArraySize(apps);
      }
  }

  while (1) {

    int ret = wait_event();

    switch(ret) {

      /*case TERMINAL_BUTTON_CLICKED:

      start_terminal();
      
      break;*/

     case SEARCH_INPUT_CLICKED:

     case SEARCH_INPUT_CHANGED:
       {

	 char input[128];

	 EM_ASM({

	     let input = document.getElementsByTagName("input")[0];

	     stringToUTF8(input.value, $0, 128);

	   }, input);

	 clearSearchResult();

	 for (int i = 0; i < nb_apps; ++i) {

	   cJSON * app = cJSON_GetArrayItem(apps, i);

	   cJSON * app_name = cJSON_GetObjectItem(app, "name");
	   cJSON * app_desc = cJSON_GetObjectItem(app, "desc");
	   cJSON * app_category = cJSON_GetObjectItem(app, "category");
	   cJSON * app_source = cJSON_GetObjectItem(app, "source");
	   cJSON * app_icon = cJSON_GetObjectItem(app, "icon");

	   if ( strcasestr(cJSON_GetStringValue(app_name), input) || strcasestr(cJSON_GetStringValue(app_desc), input) || strcasestr(cJSON_GetStringValue(app_category), input) ) {
	     cJSON * app_path = cJSON_GetObjectItem(app, "path");
	     cJSON * app_mode = cJSON_GetObjectItem(app, "mode");

	     addSearchResult(cJSON_GetStringValue(app_name), cJSON_GetStringValue(app_desc), cJSON_GetStringValue(app_category), cJSON_GetStringValue(app_source), cJSON_GetStringValue(app_icon), cJSON_GetStringValue(app_path), (app_mode != NULL)?cJSON_GetStringValue(app_mode):NULL);
	   }
	 }

	 break;
	 
       }
       
      
      case ENTER_KEY_PRESSED:
       {

	 char path[1024];
         char mode[128];

         path[0] = 0;
         mode[0] = 0;

         EM_ASM({

	  let list = document.getElementsByTagName("ul")[0].getElementsByTagName("li");

	  if (list.length == 0)
	    return;

	  stringToUTF8(list[0].path, $0, 1024);

	  if (list[0].mode)
	    stringToUTF8(list[$0].mode, $1, 128);
	  
	 }, path, mode);

	 if (path[0] != 0) {

	   if (strcmp(mode, "term") != 0) {

	     start_app("/usr/bin/havoc", path);
	   }
	   else {

	     start_app(path, NULL);
	   }
	 }

	 break;
       }

      default:
         break;
    }

    if (ret >= SEARCH_RESULT_SELECTED) {

      char path[1024];
      char mode[128];

      path[0] = 0;
      mode[0] = 0;

      EM_ASM({

	  let list = document.getElementsByTagName("ul")[0].getElementsByTagName("li");

	  if ($0 >= list.length)
	    return;

	  stringToUTF8(list[$0].path, $1, 1024);

	  if (list[$0].mode)
	    stringToUTF8(list[$0].mode, $2, 128);
	  
	}, ret-SEARCH_RESULT_SELECTED, path, mode);

      if (path[0] != 0) {

	if (strcmp(mode, "term") != 0) {

	  start_app("/usr/bin/havoc", path);
	}
	else {

	  start_app(path, NULL);
	}
      }
    }
  }

  return 0;
}

