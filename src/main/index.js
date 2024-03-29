'use strict'

import { app, BrowserWindow, dialog } from 'electron'
import { checkCookiesExpired } from '../renderer/bilibili/bilibili';
const https = require('https')
const log = require('electron-log');

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

function createWindow() {
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

  log.info(`Logfile path: ${log.transports.file.getFile().path}`)
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
  log.info('Start checking update')
  checkUpdateFromGithubAPI()
  // If cookies are expired, clean it as logout
  const store = new Store()
  const loginResponse = store.get('loginResponse', null)
  if (loginResponse !== null) {
    checkCookiesExpired(loginResponse).then(resp => {
      if (!resp['isLogin']) {
        log.info('Cookies are expired')
        store.set('loginResponse', null)
      } else {
        log.info('Cookies are valid')
      }
    })
  }
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
      log.info(`Latest version [${version}], local version [${app.getVersion()}]`)
      if (version !== 'v' + app.getVersion()) {
        log.info('Update available')
        dialog.showMessageBox(mainWindow, {
          type: 'info',
          title: '更新',
          message: '发现新版本 ' + version + '，是否前往下载？\n' + json.body,
          buttons: ['是', '否']
        }).then((result) => {
          if (result.response === 0) {
            log.info(`Update now with download url: ${json.html_url}`)
            require('openurl').open(json.html_url)
          }
        })
      }
    })
  })
  req.on('error', (e) => {
    log.error(`获取最新版本失败: `, e)
  })
  req.end()
}
