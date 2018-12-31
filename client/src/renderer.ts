// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// All of the Node.js APIs are available in this process.
import { ipcRenderer } from "electron";
import * as $ from "jquery";
import { IPost } from "./model";
import { ILoginParams } from "./socket-manager";

$("#login-submit-button").click(() => {
    const params = {
        hostname: $("#hostname-input").val() as string,
        port: $("#port-input").val() as number,
    } as ILoginParams;
    ipcRenderer.send("login-submit", params);
});

$("#add-post-button").click(() => {
    const post = {
        author: "",
        content: $("#content-input").val(),
        tags: $("#tags-input").val(),
        title: $("#title-input").val(),
    } as IPost;
    ipcRenderer.send("post", post);
});

$("#add-tag-button").click(() => {
    const tag = $("#tag-input").val();
    if (tag !== "") {
        ipcRenderer.send("subscribe", tag);
    }
});

$("#feed-button").click(() => {
    feedView();
});

$("#write-button").click(() => {
    writePostView();
});

$("#subscribe-button").click(() => {
    subscriptionsView();
});

ipcRenderer.on("init", (event: any, connected: boolean, posts: IPost[], tags: string[]) => {
    if (connected) {
        mainPage();
        posts.forEach((post) => {
            addPostToView(post);
        });
        tags.forEach((tag) => {
            addTagToView(tag);
        });
    } else {
        loginPage();
    }
});

ipcRenderer.on("login-successful", (event: any) => {
    $("#login-status").val("Success!");
    mainPage();
});

ipcRenderer.on("login-failed", (event: any) => {
    $("#login-status").val("Couldn't connect to the host");
});

ipcRenderer.on("new-post", (event: any, post: IPost) => {
    addPostToView(post);
});

ipcRenderer.on("new-tag", (event: any, tag: string) => {
    addTagToView(tag);
});

function addPostToView(post: IPost) {
    $("#posts").prepend(`<li class=\"post\">\
    <h1>${post.title}</h1>\
    <h2>By: ${post.author}</h2>\
    <p>${post.content}</p>\
    <h6>${post.tags}</h6>\
    </li>`);
}

function addTagToView(tag: string) {
    $("#tags").prepend(`<li>\
    <p>${tag}</p>\
    </li>`);
}

function loginPage() {
    showElement(".login-wrapper");
    hideElement(".main-page-wrapper");
}

function mainPage() {
    hideElement(".login-wrapper");
    showElement(".main-page-wrapper");
    $("#feed-button").addClass("page-indicator");
    $("#write-button").removeClass("page-indicator");
    $("#subscribe-button").removeClass("page-indicator");
    feedView();
}

function feedView() {
    hideElement(".write-post-wrapper");
    hideElement(".subscriptions-wrapper");
    showElement(".posts-wrapper");
}

function writePostView() {
    hideElement(".subscriptions-wrapper");
    hideElement(".posts-wrapper");
    showElement(".write-post-wrapper");
}

function subscriptionsView() {
    hideElement(".posts-wrapper");
    hideElement(".write-post-wrapper");
    showElement(".subscriptions-wrapper");
}

function showElement(selector: string) {
    $(selector).show();
    $(selector).children().show();
}

function hideElement(selector: string) {
    $(selector).children().hide();
    $(selector).hide();
}

