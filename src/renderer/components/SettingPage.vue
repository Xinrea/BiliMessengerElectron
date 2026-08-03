<template>
  <div>
    <v-dialog
      v-model="dialog"
      width="500"
    >
      <v-card v-if="stepScan">
        <v-card-title>
          {{ manualLogin.enabled ? '手动登录' : '扫码登录' }}
        </v-card-title>
        <v-card-subtitle v-if="!manualLogin.enabled">
          {{ statusText }}
        </v-card-subtitle>
        <v-card-text>
          <v-checkbox
            v-model="manualLogin.enabled"
            label="手动登录"
          ></v-checkbox>
          <v-img
            v-if="!manualLogin.enabled"
            :src="qrImage"
            max-width="160px"
          />
          <div v-if="manualLogin.enabled">
            <a
              @click="
                $electron.shell.openExternal(
                  'https://github.com/Xinrea/BiliMessengerElectron/wiki/%E6%89%8B%E5%8A%A8%E7%99%BB%E5%BD%95%E6%8C%87%E5%8D%97'
                )
              "
            >
              操作说明
            </a>
            <v-text-field
              v-model="manualLogin.SESSDATA"
              label="SESSDATA"
            />
            <v-text-field
              v-model="manualLogin.DedeUserID"
              label="DedeUserID"
            />
            <v-text-field
              v-model="manualLogin.bili_jct"
              label="bili_jct"
            />
            <v-btn
              color="primary"
              @click="manualLoginSubmit"
            >
              确认
            </v-btn>
          </div>
        </v-card-text>
      </v-card>
      <v-card v-else>
        <v-card-title>直播间号设置</v-card-title>
        <v-card-text>
          <v-text-field
            v-model="roomSetting.roomID"
            label="直播间号"
            :rules="rules"
            hide-details="auto"
            @change="roomSetting.edited = true"
          >
            <template #append-outer>
              <v-btn
                plain
                color="primary"
                :disabled="!roomSetting.edited"
                @click="saveRoomID"
              >
                保存
              </v-btn>
            </template>
          </v-text-field>
        </v-card-text>
      </v-card>
    </v-dialog>
    <v-card elevation="2">
      <v-card-text
        class="h_info"
        style="display: flex"
      >
        <div>
          <v-avatar
            color="dark"
            size="80"
            class="mr-3"
          >
            <img
              :src="getAvatarUrl()"
              alt="avatar"
            />
          </v-avatar>
        </div>
        <div
          v-if="loginResponse"
          class="d-flex flex-column align-start justify-center"
        >
          <div
            class="text-body-1 font-weight-bold pointer"
            @click="
              openURL(
                'https://space.bilibili.com/' + loginResponse['DedeUserID']
              )
            "
          >
            {{ accountData['uname'] }}
          </div>
          <div class="text-body-2">
            {{ accountData['mid'] }}
          </div>
        </div>
        <div class="ml-auto d-flex align-center">
          <v-btn
            elevation="1"
            :color="loginResponse ? 'rgb(195, 82, 82)' : 'primary'"
            @click="loginResponse ? logout() : openLoginDialog()"
          >
            {{ loginResponse ? '退出登录' : '请先登录' }}
          </v-btn>
        </div>
      </v-card-text>
    </v-card>
    <v-card class="mt-3">
      <v-card-text>
        <v-text-field
          v-model="roomSetting.roomID"
          label="直播间号"
          :rules="rules"
          hide-details="auto"
          @change="roomSetting.edited = true"
        >
          <template #append-outer>
            <v-btn
              plain
              color="primary"
              :disabled="!roomSetting.edited"
              @click="saveRoomID"
            >
              保存
            </v-btn>
          </template>
        </v-text-field>
        <v-text-field
          v-model="setting.sendInterval"
          label="发送间隔（毫秒）"
          @change="saveSetting"
        />
      </v-card-text>
    </v-card>
    <v-snackbar
      v-model="snackBar"
      timeout="1500"
      right
    >
      {{ snackBarText }}
    </v-snackbar>
  </div>
</template>

<script>
import * as https from 'https'

function parseSetCookies(setCookieHeaders, cookies) {
  let headers = setCookieHeaders || []
  headers.forEach((header) => {
    let cookie = header.split(';')[0]
    let separator = cookie.indexOf('=')
    if (separator !== -1) {
      cookies[cookie.slice(0, separator)] = cookie.slice(separator + 1)
    }
  })
}

function buildCookieString(cookies) {
  return Object.keys(cookies)
    .map((key) => key + '=' + cookies[key])
    .join('; ')
}

function createLoginResponse(cookies) {
  if (!cookies.DedeUserID || !cookies.SESSDATA || !cookies.bili_jct) {
    return null
  }
  let sessdata = cookies.SESSDATA
  try {
    sessdata = decodeURIComponent(sessdata)
  } catch (e) {
    // 保留服务端下发的原值，后续请求还会执行兼容性归一化
  }
  return Object.assign({}, cookies, {
    cookies: buildCookieString(cookies),
    SESSDATA: sessdata
  })
}

function resolveQrLogin(loginUrl, redirects, cookies) {
  redirects = redirects || 0
  cookies = cookies || {}

  return new Promise((resolve, reject) => {
    let parsedUrl
    try {
      parsedUrl = new URL(loginUrl)
    } catch (e) {
      reject(new Error('扫码登录返回了无效地址'))
      return
    }

    let loginCookieNames = [
      'SESSDATA',
      'DedeUserID',
      'DedeUserID__ckMd5',
      'bili_jct',
      'sid'
    ]
    loginCookieNames.forEach((key) => {
      if (parsedUrl.searchParams.has(key)) {
        cookies[key] = parsedUrl.searchParams.get(key)
      }
    })
    let loginResponse = createLoginResponse(cookies)
    if (loginResponse) {
      resolve(loginResponse)
      return
    }
    if (redirects > 5) {
      reject(new Error('扫码登录跳转次数过多'))
      return
    }

    if (parsedUrl.protocol !== 'https:') {
      reject(new Error('扫码登录跳转到了不安全的地址'))
      return
    }
    let requestOptions = {
      hostname: parsedUrl.hostname,
      port: parsedUrl.port || 443,
      path: parsedUrl.pathname + parsedUrl.search,
      method: 'GET',
      headers: {
        cookie: buildCookieString(cookies),
        Referer: 'https://passport.bilibili.com/',
        'User-Agent':
          'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36'
      },
      rejectUnauthorized: true
    }
    let request = https.get(
      requestOptions,
      (res) => {
        parseSetCookies(res.headers['set-cookie'], cookies)
        let response = createLoginResponse(cookies)
        if (response) {
          res.resume()
          resolve(response)
          return
        }

        let location = res.headers.location
        res.resume()
        if (location) {
          resolve(resolveQrLogin(new URL(location, parsedUrl).toString(), redirects + 1, cookies))
        } else {
          reject(new Error('登录成功，但未能取得账号 Cookie，请重新扫码'))
        }
      }
    )
    request.on('error', reject)
  })
}

export default {
  name: 'SettingPage',
  data() {
    return {
      dialog: false,
      stepScan: true,
      manualLogin: {
        enabled: false,
        SESSDATA: '',
        DedeUserID: '',
        bili_jct: ''
      },
      qrImage: '',
      statusText: '请使用 bilibili 手机 App 扫描下方二维码',
      qrTimer: null,
      accountData: {
        type: Object,
        default: null
      },
      rules: [
        (value) => !!value || '必须填写直播间号',
        (value) => Number.isInteger(Number(value)) || '直播间号必须为数字'
      ],
      snackBar: false,
      snackBarText: '',
      setting: {
        sendInterval: 1000
      },
      roomSetting: {
        roomID: '',
        edited: false
      },
      loginResponse: null
    }
  },
  watch: {
    dialog(val) {
      if (val === false) {
        clearInterval(this.qrTimer)
      }
    }
  },
  mounted() {
    this.loginResponse = this.Store.get('loginResponse', null)
    this.roomSetting.roomID = this.Store.get('roomID', '')
    this.setting = this.Store.get('setting', {
      sendInterval: 1000
    })
    if (this.loginResponse !== null) {
      this.updateUserInfo(this.loginResponse['DedeUserID'])
    }
  },
  methods: {
    getAvatarUrl() {
      if (this.loginResponse == null || this.accountData['face'] == null) {
        return 'static/noface.jpg'
      }
      return 'https:' + this.accountData['face']
    },
    updateUserInfo(uid) {
      let that = this
      this.Bilibili.getUserInfo(this.loginResponse, uid)
        .then((resp) => {
          that.accountData = resp
        })
        .catch((err) => {
          console.log(err)
        })
    },
    manualLoginSubmit() {
      let that = this
      that.loginResponse = {
        SESSDATA: decodeURIComponent(that.manualLogin.SESSDATA),
        DedeUserID: parseInt(that.manualLogin.DedeUserID),
        bili_jct: that.manualLogin.bili_jct
      }
      that.Store.set('loginResponse', that.loginResponse)
      that.updateUserInfo(that.manualLogin.DedeUserID)
      // Display Room number
      {
        that.roomSetting.edited = true
        that.stepScan = false
      }
    },
    openLoginDialog() {
      let that = this
      that.statusText = '请使用 bilibili 手机 App 扫描下方二维码'
      that.dialog = true
      https.get(
        'https://passport.bilibili.com/x/passport-login/web/qrcode/generate',
        (res) => {
          res.on('data', (chunk) => {
            let resp = JSON.parse(chunk.toString())
            let QRCode = require('qrcode')
            // eslint-disable-next-line handle-callback-err
            QRCode.toDataURL(resp['data']['url'], function (err, url) {
              that.qrImage = url
            })
            // https://passport.bilibili.com/x/passport-login/web/qrcode/poll
            let qrcodeKey = resp['data']['qrcode_key']
            let pollOptions = {
              hostname: 'passport.bilibili.com',
              path: '/x/passport-login/web/qrcode/poll?qrcode_key=' + qrcodeKey,
              method: 'GET'
            }
            that.qrTimer = setInterval(() => {
              let statusReq = https.request(pollOptions, (res) => {
                let dd = ''
                res.on('data', (secCheck) => {
                  dd += secCheck
                })
                res.on('end', () => {
                  let resp = JSON.parse(dd)
                  if (resp['data']['code'] === 0) {
                    clearInterval(that.qrTimer)
                    that.statusText = '正在完成登录...'
                    resolveQrLogin(resp['data']['url'])
                      .then((loginResponse) => {
                        that.statusText = '登录成功'
                        that.loginResponse = loginResponse
                        that.Store.set('loginResponse', loginResponse)
                        that.updateUserInfo(loginResponse.DedeUserID)
                        // Display Room number
                        that.roomSetting.edited = true
                        that.stepScan = false
                      })
                      .catch((err) => {
                        that.statusText = '登录失败：' + err.message
                      })
                  } else {
                    if (resp['data']['code'] === 86101) {
                      that.statusText =
                        '请使用 bilibili 手机 App 扫描下方二维码'
                    } else if (resp['data']['code'] === 86090) {
                      that.statusText = '请在手机上点击 确认登录'
                    } else {
                      that.statusText =
                        '确认状态失败，请稍候重试:\n' +
                        JSON.stringify(resp['data'])
                    }
                  }
                })
              })
              statusReq.end()
            }, 5000)
          })
        }
      )
    },
    openURL(url) {
      let shell = require('electron').shell
      shell.openExternal(url)
    },
    saveRoomID() {
      let that = this
      this.Bilibili.getRoomInfo(this.roomSetting.roomID)
        .then(() => {
          that.roomSetting.edited = false
          that.Store.set('roomID', that.roomSetting.roomID)
          that.showSnackBar('直播间号保存成功')
          that.dialog = false
          that.stepScan = true
        })
        .catch(() => {
          that.showSnackBar('不存在的直播间，请检查是否填写正确')
        })
    },
    logout() {
      this.Store.set('loginResponse', null)
      this.loginResponse = null
    },
    showSnackBar(text) {
      this.snackBarText = text
      this.snackBar = true
    },
    saveSetting() {
      this.Store.set('setting', this.setting)
    }
  }
}
</script>

<style>
.pointer {
  cursor: pointer;
}
</style>
