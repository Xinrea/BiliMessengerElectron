<template>
  <div>
    <v-dialog
      v-model="dialog"
      width="500"
    >
      <v-card>
        <v-card-title>扫码登录</v-card-title>
        <v-card-subtitle>{{ statusText }}</v-card-subtitle>
        <v-card-text>
          <v-img
            :src="qrImage"
            max-width="160px"
          />
        </v-card-text>
      </v-card>
    </v-dialog>
    <v-card
      elevation="2"
    >
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
            <img :src="getAvatarUrl()">
          </v-avatar>
        </div>

        <div v-if="!isSet">
          <v-btn
            elevation="1"
            @click="openLoginDialog()"
          >
            请先登录
          </v-btn>
        </div>
        <div
          v-else
          class="d-flex flex-column align-start justify-center"
        >
          <div
            class="text-body-1 font-weight-bold pointer"
            @click="openURL('https://space.bilibili.com/'+userdata['DedeUserID'])"
          >
            {{ accountData['name'] }}
          </div>
          <div class="text-body-2">
            {{ accountData['mid'] }}
          </div>
        </div>
      </v-card-text>
    </v-card>
    <v-card class="mt-3">
      <v-card-text>
        <v-sheet>
          <v-text-field
            label="直播间号"
            :value="roomEditValue"
            :rules="rules"
            hide-details="auto"
            @change="roomEdit = true"
          >
            <template #append-outer>
              <v-btn
                plain
                color="primary"
                :disabled="!roomEdit"
                @click="saveRoomID"
              >
                保存
              </v-btn>
            </template>
          </v-text-field>
        </v-sheet>
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

export default {
  name: 'SettingPage',
  props: {
    isSet: {
      type: Boolean,
      default: false
    },
    userdata: {
      type: Object,
      default: null
    }
  },
  data () {
    return {
      dialog: false,
      qrImage: '',
      statusText: '请使用 bilibili 手机 App 扫描下方二维码',
      qrTimer: null,
      accountData: {
        type: Object,
        default: null
      },
      rules:[
        value => !!value || '必须填写直播间号',
        value => Number.isInteger(Number(value)) || "直播间号必须为数字",
      ],
      roomEditValue: "21484828",
      roomEdit: false,
      roomID: "",
      snackBar: false,
      snackBarText: ""
    }
  },
  watch: {
    dialog (val) {
      if (val === false) {
        clearInterval(this.qrTimer)
      }
    },
    isSet (val) {
      if (val === true) {
        this.updateUserInfo(this.userdata['DedeUserID'])
      }
    }
  },
  mounted () {
    console.log(this.userdata)
    if (this.userdata !== null) {
      this.updateUserInfo(this.userdata['DedeUserID'])
    }
  },
  methods: {
    getAvatarUrl () {
      if (this.accountData === null) {
        return 'static/noface.jpg'
      }
      return this.accountData['face']
    },
    updateUserInfo (uid) {
      // https://api.bilibili.com/x/space/acc/info?mid=1581869085&jsonp=jsonp
      let that = this
      this.Bilibili.getUserInfo(uid, resp=>{
        that.accountData = resp
      })
      // https.get('https://api.bilibili.com/x/space/acc/info?mid=' + uid + '&jsonp=jsonp', res => {
      //   res.on('data', chunk => {
      //     console.log(chunk.toString())
      //     let resp = JSON.parse(chunk.toString())
      //     if (resp['code'] == 0) {
      //       that.accountData = resp['data']
      //     }
      //   })
      // })
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
    },
    openURL (url) {
      let shell = require('electron').shell
      shell.openExternal(url)
    },
    saveRoomID () {
      this.roomID = this.roomEditValue
      this.roomEdit = false
      this.snackBarText = "直播间号设置成功"
      this.snackBar = true
    } 
  }
}
</script>

<style>

.pointer {
  cursor: pointer;
}

</style>
