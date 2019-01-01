import { BrowserWindow, ipcRenderer } from "electron";
import { connect, Socket } from "net";
import { IPost, IServerMessage } from "./model";

export interface ILoginParams {
    hostname: string;
    port: number;
    username: string;
}

export class SocketManager {
    public userID: number;
    public username: string;
    public posts: IPost[];
    public tags: string[];
    private socket: Socket;
    private message: string;
    private connected: boolean;

    constructor(hostname: string, port: number, private window: BrowserWindow) {
        this.connected = false;
        this.message = "";
        this.posts = [];
        this.tags = [];
        try {
            this.socket = connect(port, hostname);
            this.socket.on("data", (data) => this.processMessage(data.toString()));
            this.socket.on("close", () => { this.connected = false; });
            this.connected = true;
        } catch (error) {
            this.window.webContents.send("login-falied", "Could not connect to the server");
        }
    }

    public login(username: string) {
        const message = {
            author: username,
            type: 0,
        } as IServerMessage;
        this.username = username;
        this.sendMessage(message);
    }

    public isConnected() {
        return this.connected;
    }

    public setWindow(window: BrowserWindow) {
        this.window = window;
    }

    public subscribeTag(tag: string) {
        const message = {
            content: tag,
            type: 20,
            userID: this.userID,
        } as IServerMessage;
        this.sendMessage(message);
    }

    public unsubscribeTag(tag: string) {
        const message = {
            content: tag,
            type: 30,
            userID: this.userID,
        } as IServerMessage;
        this.sendMessage(message);
    }

    public sendMessage(message: IServerMessage) {
        let messageStr = `{"type": ${message.type},`;
        if (message.userID === undefined) {
            if (this.userID === undefined) {
                message.userID = -1;
            } else {
                message.userID = this.userID;
            }
        }
        messageStr += `"userID": ${message.userID},\
        "title": "${message.title}",\
        "author": "${message.author}",\
        "content": "${message.content}",\
        "tags": "${message.tags}"}\n`;
        // tslint:disable-next-line:no-console
        console.log(messageStr);
        this.socket.write(messageStr);
    }

    private processMessage(data: string) {
        data = data.replace(/\0/g, "");
        this.message += data;
        const msg = JSON.parse(this.message) as IServerMessage;
        this.message = "";
        switch (msg.type) {
            case 10: {
                this.addPost({
                    author: msg.author,
                    content: msg.content,
                    tags: msg.tags,
                    title: msg.title,
                } as IPost);
                break;
            }
            case 1: {
                this.userID = msg.userID;
                this.window.webContents.send("login-successful", this.username);
                break;
            }
            case 2: {
                this.username = "";
                this.window.webContents.send("login-failed", "Cannot log in on the server");
                break;
            }
            case 21: {
                this.window.webContents.send("subscribe-succesful", msg.content);
                break;
            }
            case 22 || 23: {
                this.window.webContents.send("subscribe-failed", msg.content);
                break;
            }
            case 31: {
                this.window.webContents.send("unsubscribe-success", msg.content);
                break;
            }
            case 32 || 33: {
                this.window.webContents.send("unsubscribe-failed", msg.content);
                break;
            }
            default:
                // tslint:disable-next-line:no-console
                console.log(JSON.stringify(msg));
                break;
        }
    }

    private addPost(post: IPost) {
        this.posts.push(post);
        this.window.webContents.send("new-post", post);
    }
}
