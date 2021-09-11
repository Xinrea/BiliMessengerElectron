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
            <img
              :src="getAvatarUrl()"
              alt="avatar"
            >
          </v-avatar>
        </div>
        <div
          v-if="isSet"
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
          <div class="text-body-2">
            {{ accountData['sign'] }}
          </div>
        </div>
        <div class="ml-auto d-flex align-center">
          <v-btn
            elevation="1"
            :color="isSet?'rgb(195, 82, 82)':'primary'"
            @click="isSet?logout():openLoginDialog()"
          >
            {{ isSet ? '退出登录':'请先登录' }}
          </v-btn>
        </div>
      </v-card-text>
    </v-card>
    <v-card class="mt-3">
      <v-card-text>
        <v-sheet>
          <v-text-field
            v-model="roomEditValue"
            label="直播间号"
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
    },
    rid: {
      type: String,
      default: "2"
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
    },
    rid (val) {
      this.roomEditValue = val
    }
  },
  mounted () {
    if (this.isSet) {
      this.updateUserInfo(this.userdata['DedeUserID'])
    }
    this.roomEditValue = this.Store.get('roomID', '21484828')
  },
  methods: {
    getAvatarUrl () {
      if (!this.isSet) {
        return 'static/noface.jpg'
      }
      return this.accountData['face']
    },
    updateUserInfo (uid) {
      let that = this
      this.Bilibili.getUserInfo(uid, resp=>{
        that.accountData = resp
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
      let that = this
      console.log(this.roomEditValue, this.rid)
      this.Bilibili.getRoomInfo(this.roomEditValue).then(()=>{
        that.$emit('updateRoomID', that.roomEditValue)
        that.roomEdit = false
        that.showSnackBar('直播间号保存成功')
      }).catch(()=>{
          that.showSnackBar('不存在的直播间，请检查是否填写正确')
      })
    },
    logout () {
      this.$emit('logout')
    },
    showSnackBar(text) {
        this.snackBarText = text
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
