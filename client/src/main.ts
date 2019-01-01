import { app, BrowserWindow, ipcMain } from "electron";
import * as path from "path";
import { IPost, IServerMessage } from "./model";
import { ILoginParams, SocketManager } from "./socket-manager";

let mainWindow: Electron.BrowserWindow;
let socketManager: SocketManager;

function createWindow() {
  // Create the browser window.
  mainWindow = new BrowserWindow({
    height: 620,
    width: 900,
  });

  // and load the index.html of the app.
  mainWindow.loadFile(path.join(__dirname, "../index.html"));

  // Open the DevTools.
  // mainWindow.webContents.openDevTools();
  mainWindow.webContents.on("did-finish-load", () => {
    const connected = socketManager === undefined ? false : socketManager.isConnected();
    if (connected) {
      socketManager.setWindow(mainWindow);
      mainWindow.webContents.send("init", connected, socketManager.posts, socketManager.tags);
    }
    mainWindow.webContents.send("init", connected, [], []);
  });
  // Emitted when the window is closed.
  mainWindow.on("closed", () => {
    // Dereference the window object, usually you would store windows
    // in an array if your app supports multi windows, this is the time
    // when you should delete the corresponding element.
    mainWindow = null;
  });
}

// This method will be called when Electron has finished
// initialization and is ready to create browser windows.
// Some APIs can only be used after this event occurs.
app.on("ready", createWindow);

// Quit when all windows are closed.
app.on("window-all-closed", () => {
  // On OS X it is common for applications and their menu bar
  // to stay active until the user quits explicitly with Cmd + Q
  if (process.platform !== "darwin") {
    app.quit();
  }
});

app.on("activate", () => {
  // On OS X it"s common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (mainWindow === null) {
    createWindow();
  }
});

// In this file you can include the rest of your app"s specific main process
// code. You can also put them in separate files and require them here.
ipcMain.on("login-submit", (event: any, params: ILoginParams) => {
  socketManager = new SocketManager(params.hostname, params.port, mainWindow);
  if (socketManager.isConnected()) {
    socketManager.login(params.username);
  } else {
    event.sender.send("login-failed", "Cannot connect to the server");
  }
});

// ipcMain.on("send-message", (event: any, message: string) => {
//   socketManager.sendMessage(message);
// });

ipcMain.on("post", (event: any, post: IPost) => {
  post.author = socketManager.username;
  socketManager.sendMessage({
    author: post.author,
    content: post.content,
    tags: post.tags,
    title: post.title,
    type: 10,
    userID: socketManager.userID,
  } as IServerMessage);
  mainWindow.webContents.send("new-post", post);
});

ipcMain.on("subscribe", (event: any, tag: string) => {
  socketManager.subscribeTag(tag);
  // mainWindow.webContents.send("new-tag", tag);
});

ipcMain.on("unsubscribe", (event: any, tag: string) => {
  socketManager.unsubscribeTag(tag);
});
