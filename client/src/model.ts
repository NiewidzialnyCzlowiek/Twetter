export interface IServerMessage {
    type: number;
    userID: number;
    title: string;
    author: string;
    content: string;
    tags: string;
}

export interface IPost {
    author: string;
    title: string;
    content: string;
    tags: string;
}
