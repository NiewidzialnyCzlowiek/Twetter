// This file is required by the index.html file and will
// be executed in the renderer process for that window.
// All of the Node.js APIs are available in this process.
import { ipcRenderer } from "electron";
import * as $ from "jquery";
import { IPost } from "./model";
import { ILoginParams } from "./socket-manager";


$("#login-submit-button").click(() => {
    const defaultHostname = "localhost";
    const defaultPort = 1234;
    const defaultUsername = "anon" + (Math.floor(Math.random() * (9000 - 1000 + 1)) + 1000).toString();
    const params = {
        hostname: $("#hostname-input").val() as string,
        port: $("#port-input").val() as number,
        username: $("#username-input").val(),
    } as ILoginParams;
    if (params.hostname === "" || params.hostname === undefined) {
        params.hostname = defaultHostname;
    }
    if (params.port.toString() === "" || params.port === undefined) {
        params.port = defaultPort;
    }
    if (params.username === "" || params.username === undefined) {
        params.username = defaultUsername;
    }
    ipcRenderer.send("login-submit", params);
});

$("#add-post-button").click(() => {
    const post = {
        content: $("#content-input").val(),
        tags: $("#tags-input").val(),
        title: $("#title-input").val(),
    } as IPost;
    $("#content-input").val("");
    $("#tags-input").val("");
    $("#title-input").val("");
    ipcRenderer.send("post", post);
});

$("#add-tag-button").click(() => {
    const tag = $("#tag-input").val();
    if (tag !== "") {
        $("#tag-input").val("");
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

ipcRenderer.on("login-successful", (event: any, username: string) => {
    $("#login-status").text("");
    $("head").children("title").remove();
    $("head").append(`<title>${username}</title>`);
    mainPage();
});

ipcRenderer.on("login-failed", (event: any, errorMessage: string) => {
    $("#login-status").text(errorMessage);
});

ipcRenderer.on("new-post", (event: any, post: IPost) => {
    addPostToView(post);
});

ipcRenderer.on("subscribe-succesful", (event: any, tag: string) => {
    addTagToView(tag);
});

ipcRenderer.on("unsubscribe-success", (event: any, tag: string) => {
    $(`#${tag}-li`).remove();
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
    $("#tags").prepend(`<li id="${tag}-li">\
    <p>${tag}</p>\
    <button id="unsubscribe-${tag}-button">Unsub</button>
    </li>`);
    $(`#unsubscribe-${tag}-button`).click(() => {
        ipcRenderer.send("unsubscribe", tag);
    });
}

function loginPage() {
    showElement(".login-wrapper");
    hideElement(".main-page-wrapper");
}

function mainPage() {
    hideElement(".login-wrapper");
    showElement(".main-page-wrapper");
    feedView();
}

function feedView() {
    hideElement(".write-post-wrapper");
    hideElement(".subscriptions-wrapper");
    showElement(".posts-wrapper");
    $("#feed-button").addClass("page-indicator");
    $("#write-button").removeClass("page-indicator");
    $("#subscribe-button").removeClass("page-indicator");
}

function writePostView() {
    hideElement(".subscriptions-wrapper");
    hideElement(".posts-wrapper");
    showElement(".write-post-wrapper");
    $("#feed-button").removeClass("page-indicator");
    $("#write-button").addClass("page-indicator");
    $("#subscribe-button").removeClass("page-indicator");
}

function subscriptionsView() {
    hideElement(".posts-wrapper");
    hideElement(".write-post-wrapper");
    showElement(".subscriptions-wrapper");
    $("#feed-button").removeClass("page-indicator");
    $("#write-button").removeClass("page-indicator");
    $("#subscribe-button").addClass("page-indicator");
}

function showElement(selector: string) {
    $(selector).show();
    $(selector).children().show();
}

function hideElement(selector: string) {
    $(selector).children().hide();
    $(selector).hide();
}

