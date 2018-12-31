import { BrowserWindow, ipcRenderer } from "electron";
import { connect, Socket } from "net";
import { IPost, IServerMessage } from "./model";

export interface ILoginParams {
    hostname: string;
    port: number;
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
            // tslint:disable-next-line:no-console
            console.log("Could not connect to the server");
        }
    }

    public isConnected() {
        return this.connected;
    }

    public setWindow(window: BrowserWindow) {
        this.window = window;
    }

    public subscribeTag(tag: string): any {
        const message = {
            content: tag,
            type: 20,
            userID: this.userID,
        } as IServerMessage;
        this.sendMessage(message);
    }

    public sendMessage(message: IServerMessage) {
        this.socket.write(JSON.stringify(message) + "\n");
    }

    private processMessage(data: string) {
        data = data.replace(/\0/g, "");
        this.message += data;
        const msg = JSON.parse(this.message) as IServerMessage;
        this.message = "";
        if (msg.type === 10) {
            this.addPost({
                author: msg.author,
                content: msg.content,
                tags: msg.tags,
                title: msg.title,
            } as IPost);
        }
    }

    private addPost(post: IPost) {
        this.posts.push(post);
        this.window.webContents.send("new-post", post);
    }
}
