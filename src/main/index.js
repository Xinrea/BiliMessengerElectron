'use strict'

import { app, BrowserWindow, dialog } from 'electron'
const https = require('https')

/**
 * Set `__static` path to static files in production
 * https://simulatedgreg.gitbooks.io/electron-vue/content/en/using-static-assets.html
 */
if (process.env.NODE_ENV !== 'development') {
  global.__static = require('path')
    .join(__dirname, '/static')
    .replace(/\\/g, '\\\\')
}

require('@electron/remote/main').initialize()

let mainWindow
const winURL =
  process.env.NODE_ENV === 'development'
    ? 'http://localhost:9080'
    : `file://${__dirname}/index.html`

function createWindow () {
  /**
   * Initial window options
   */
  mainWindow = new BrowserWindow({
    height: 720,
    minHeight: 720,
    useContentSize: true,
    width: 1280,
    minWidth: 1280,
    webPreferences: {
      webSecurity: false,
      nodeIntegration: true,
      enableRemoteModule: true,
      contextIsolation: false
    }
  })

  require("@electron/remote/main").enable(mainWindow.webContents)

  mainWindow.loadURL(winURL)

  mainWindow.on('closed', () => {
    mainWindow = null
  })
}

app.on('ready', createWindow)

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('activate', () => {
  if (mainWindow === null) {
    createWindow()
  }
})

// Init storage
const Store = require('electron-store');
Store.initRenderer();

/**
 * Auto Updater
 *
 * Uncomment the following code below and install `electron-updater` to
 * support auto updating. Code Signing with a valid certificate is required.
 * https://simulatedgreg.gitbooks.io/electron-vue/content/en/using-electron-builder.html#auto-updating
 */

/*
import { autoUpdater } from 'electron-updater'

autoUpdater.on('update-downloaded', () => {
  autoUpdater.quitAndInstall()
})

 */

app.on('ready', () => {
  console.log("Start checking update")
  checkUpdateFromGithubAPI()
})

function checkUpdateFromGithubAPI() {
  const options = {
    hostname: 'api.github.com',
    port: 443,
    path: '/repos/xinrea/BiliMessengerElectron/releases/latest',
    method: 'GET',
    headers: {
      'User-Agent': 'request'
    }
  }
  const req = https.request(options, (res) => {
    let data = ''
    res.on('data', (d) => {
      data += d
    })
    res.on('end', () => {
      let json = JSON.parse(data)
      let version = json.tag_name
      console.log('latest version:', version, 'current version:', app.getVersion())
      if (version !== 'v'+app.getVersion()) {
        console.log('Update available')
        dialog.showMessageBox(mainWindow, {
          type: 'info',
          title: '更新',
          message: '发现新版本 '+version+'，是否前往下载？\n'+json.body,
          buttons: ['是', '否']
        }).then((result) => {
          if (result.response === 0) {
            console.log("Update now")
            require('openurl').open(json.html_url)
          }
        })
      }
    })
  })
  req.on('error', (e) => {
    console.error(e)
  })
  req.end()
}