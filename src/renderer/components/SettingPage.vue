<template>
  <div>
      <v-dialog
          v-model="dialog"
          width="500"
      >
        <v-card>
          <v-card-title>扫码登录</v-card-title>
          <v-card-subtitle>{{statusText}}</v-card-subtitle>
          <v-card-text>
            <v-img :src="qrImage" max-width="160px"></v-img>
          </v-card-text>
        </v-card>
      </v-dialog>
    <v-card
        elevation="2"
    >
      <v-card-text>
            <v-avatar
                color="dark"
                size="62"
            ><img :src="avatarUrl"></v-avatar>
            <v-btn elevation="1" style="margin-left: 10px" @click="openLoginDialog()" color="secondary">登录</v-btn>
      </v-card-text>
    </v-card>
  </div>
</template>

<script>
import * as https from 'https'

export default {
  name: 'SettingPage',
  props: ['userdata'],
  data () {
    return {
      dialog: false,
      qrImage: '',
      statusText: '请使用 bilibili 手机 App 扫描下方二维码',
      qrTimer: null,
      avatarUrl: 'static/noface.jpg'
    }
  },
  mounted () {
    if (this.userdata !== null) {
      this.updateUserInfo(this.userdata['DedeUserID'])
    }
  },
  watch: {
    dialog (val, oldval) {
      if (val === false) {
        clearInterval(this.qrTimer)
      }
    }
  },
  methods: {
    updateUserInfo (uid) {
      // https://api.bilibili.com/x/space/acc/info?mid=1581869085&jsonp=jsonp
      let that = this
      https.get('https://api.bilibili.com/x/space/acc/info?mid=' + uid + '&jsonp=jsonp', res => {
        res.on('data', chunk => {
          let resp = JSON.parse(chunk.toString())
          that.avatarUrl = resp['data']['face']
        })
      })
    },
    openLoginDialog () {
      let that = this
      that.statusText = '请使用 bilibili 手机 App 扫描下方二维码'
      that.dialog = true
      https.get('https://passport.bilibili.com/qrcode/getLoginUrl', res => {
        res.on('data', chunk => {
          let resp = JSON.parse(chunk.toString())
          let QRCode = require('qrcode')
          // eslint-disable-next-line handle-callback-err
          QRCode.toDataURL(resp['data']['url'], function (err, url) {
            that.qrImage = url
          })
          // http://passport.bilibili.com/qrcode/getLoginInfo
          let postData = 'oauthKey=' + resp['data']['oauthKey']
          let postOptions = {
            hostname: 'passport.bilibili.com',
            path: '/qrcode/getLoginInfo',
            method: 'POST',
            headers: {
              'Content-Type': 'application/x-www-form-urlencoded',
              'Content-Length': Buffer.byteLength(postData)
            }
          }
          that.qrTimer = setInterval(() => {
            console.log('timer running')
            let statusReq = https.request(postOptions, res => {
              let dd = ''
              res.on('data', secCheck => {
                dd += secCheck
              })
              res.on('end', () => {
                let resp = JSON.parse(dd)
                // {"status":false,"data":-4,"message":"Can't scan~"}
                if (resp['status'] === true) {
                  that.statusText = '登录成功'
                  that.dialog = false
                  let querystring = require('querystring')
                  let url = resp['data']['url']
                  let params = querystring.parse(url.split('?')[1])
                  that.$emit('login-success', params)
                  that.updateUserInfo(params['DedeUserID'])
                } else {
                  if (resp['data'] === -4) {
                    that.statusText = '请使用 bilibili 手机 App 扫描下方二维码'
                  } else if (resp['data'] === -5) {
                    that.statusText = '请在手机上点击 确认登录'
                  } else {
                    that.statusText = '确认状态失败，请稍候重试'
                  }
                }
              })
            })
            statusReq.write(postData)
            statusReq.end()
          }, 5000)
        })
      })
    }
  }
}
</script>

<style>

</style>
