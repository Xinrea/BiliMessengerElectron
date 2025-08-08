<template>
  <div>
    <setting-alert-page v-if="!isLogin" />
    <div v-else>
      <v-card>
        <v-card-title> 模板选择 </v-card-title>
        <v-card-text>
          <v-select
            v-model="templateName"
            :items="templateList"
            label="模板"
          />
          <v-textarea
            readonly
            no-resize
            :value="getTemplateValue()"
            label="模板内容"
            class="template-textarea"
            outlined
          />
          <div v-if="getTemplateValue().indexOf('{code}') !== -1">
            该模板需要使用兑换码
          </div>
        </v-card-text>
      </v-card>
      <v-card class="mt-3">
        <v-card-title> 发送 </v-card-title>
        <v-card-text>
          <p>当前发送进度：{{ progressIndex }} / {{ guardLength }}</p>
          <p>
            私信目标：
            <span
              v-for="(g, i) in getShortList()"
              :key="i"
              class="ml-1"
            >
              <v-chip :color="i === 0 ? 'orange' : 'default'">
                {{ g.username }}
              </v-chip>
            </span>
            <span>
              <v-chip v-if="guardLength - progressIndex > 5"> ... </v-chip>
            </span>
          </p>
          <v-textarea
            readonly
            no-resize
            :value="getMessageContent()"
            label="私信内容"
            class="message-textarea"
            outlined
          />
          <v-btn
            :disabled="checkBeforeSend()"
            color="primary"
            :loading="inProgress"
            @click="startSending"
          >
            开始发送
          </v-btn>
          <span
            v-if="checkBeforeSend()"
            class="ml-3"
          >
            <v-icon v-if="checkBeforeSend()">
              {{ Mdi.mdiAlertCircle }}
            </v-icon>
            {{ errorMessage }}
          </span>
        </v-card-text>
      </v-card>
      <v-card class="mt-3">
        <v-card-title> 撤回消息 </v-card-title>
        <v-card-text>
          <p>
            本次已私信用户：
            <span
              v-for="(g, i) in shortSentMsgList"
              :key="i"
              class="ml-1"
            >
              <v-chip :color="i === 0 ? 'orange' : 'default'">
                {{ g.username }}
              </v-chip>
            </span>
            <span>
              <v-chip v-if="sentMsgList.length > 5"> ... </v-chip>
            </span>
          </p>
          <v-btn
            :disabled="checkBeforeReCall"
            color="primary"
            :loading="inProgress"
            @click="startRecalling"
          >
            撤回已发消息
          </v-btn>
        </v-card-text>
      </v-card>
    </div>
  </div>
</template>

<script>
import SettingAlertPage from './SettingAlertPage.vue'
export default {
  name: 'MessagePage',
  components: { SettingAlertPage },
  data() {
    return {
      templateList: [],
      templateName: '',
      guardLength: 0,
      guardList: [],
      remainList: [],
      codeList: [],
      sentMsgList: [],
      progressIndex: 0,
      setting: {
        sendInterval: 1000
      },
      errorMessage: 'error',
      inProgress: false,
      isLogin: false
    }
  },
  computed: {
    checkBeforeReCall() {
      return this.sentMsgList.length === 0 || this.inProgress
    },
    shortSentMsgList() {
      return this.sentMsgList.slice(0, 5)
    }
  },
  mounted() {
    this.templateList = this.Store.get('templateList', [])
    this.Store.onDidChange('templateList', (v) => {
      this.templateList = v
    })
    this.guardList = this.Store.get('guards', [])
    this.Store.onDidChange('guards', (v) => {
      this.guardList = v
      if (!this.inProgress) {
        this.progressIndex = 0
        this.guardLength = this.guardList.length
      }
    })
    this.guardLength = this.guardList.length
    this.codeList = this.Store.get('codes', [])
    this.Store.onDidChange('codes', (v) => {
      this.codeList = v
    })
    this.setting = this.Store.get('setting', {
      sendInterval: 1000
    })
    this.Store.onDidChange('setting', (v) => {
      this.setting = v
    })
    this.isLogin = this.Store.get('loginResponse', null) !== null
    this.Store.onDidChange('loginResponse', (v) => {
      console.log('login changed', v)
      if (v !== null) {
        this.isLogin = true
      } else {
        this.isLogin = false
      }
    })
  },
  methods: {
    getTemplateValue() {
      return this.Store.get('template:' + this.templateName, '')
    },
    getShortList() {
      return this.guardList.slice(0, 5)
    },
    checkBeforeSend() {
      let template = this.getTemplateValue()
      if (template.indexOf('{code}') !== -1) {
        if (this.codeList.length < this.guardList.length) {
          this.errorMessage = '兑换码数量不足，请添加兑换码'
          return true
        }
      }
      if (template.length === 0) {
        this.errorMessage = '请先选择模板'
        return true
      }
      this.errorMessage = '将自动移除发送过的舰长和使用过的兑换码'
      return false
    },
    getMessageContent() {
      return this.processTag(this.getTemplateValue())
    },
    processTag(v) {
      if (
        this.guardLength === 0 ||
        this.progressIndex > this.guardLength ||
        this.guardList.length === 0
      ) {
        return ''
      }
      let dates = this.getCurrentDate()
      let currentUser = this.guardList[0]
      return v
        .replaceAll('{level}', this.guardLevelToString(currentUser.guard_level))
        .replaceAll('{name}', currentUser.username)
        .replaceAll('{code}', this.codeList[0])
        .replaceAll('{date}', dates.fullDate)
        .replaceAll('{month}', dates.month.toString())
        .replaceAll('{lastMonth}', dates.lastMonth.toString())
        .replaceAll('{day}', dates.day.toString())
    },
    guardLevelToString(level) {
      if (level === level + '') {
        return level
      }
      switch (level) {
        case 1: {
          return '总督'
        }
        case 2: {
          return '提督'
        }
        case 3: {
          return '舰长'
        }
        default: {
          return '自定义'
        }
      }
    },
    getCurrentDate() {
      let date = new Date()
      let mon = date.getMonth() + 1
      let day = date.getDate()
      return {
        fullDate:
          date.getFullYear() +
          '-' +
          (mon < 10 ? '0' + mon : mon) +
          '-' +
          (day < 10 ? '0' + day : day),
        month: date.getMonth() + 1,
        lastMonth: date.getMonth() === 0 ? 12 : date.getMonth(),
        day: date.getDate()
      }
    },
    startSending() {
      console.log('start')
      this.inProgress = true
      this.sentMsgList = []
      this.prosessNext()
    },
    prosessNext() {
      console.log('process', this.progressIndex, this.guardLength)
      if (this.progressIndex < this.guardLength) {
        this.Bilibili.sendMessage(
          this.guardList[0].uid,
          this.Store.get('loginResponse'),
          this.getMessageContent()
        )
          .then((res) => {
            this.progressIndex += 1
            // 保存发送消息接口返回的msg_key用于撤回
            this.sentMsgList.push({
              uid: this.guardList[0].uid,
              username: this.guardList[0].username,
              msg_key: res.msg_key,
              timestamp: new Date().getTime()
            })
            this.guardList.splice(0, 1)
            this.Store.set('guards', this.guardList)
            if (this.getTemplateValue().indexOf('{code}') !== -1) {
              this.codeList.splice(0, 1)
              this.Store.set('codes', this.codeList)
            }
            setTimeout(this.prosessNext, this.setting.sendInterval)
          })
          .catch((e) => {
            var currentGuard = this.guardList[0]
            this.remainList.push(currentGuard)
            this.progressIndex += 1
            this.guardList.splice(0, 1)
            this.Store.set('guards', this.guardList)
            new Notification('私信发送失败', {
              body:
                '向' +
                currentGuard.username +
                '发送私信时遇到了问题：' +
                e.message
            })
            setTimeout(this.prosessNext, this.setting.sendInterval)
          })
      } else {
        // End of message sending
        this.inProgress = false
        this.Store.set('guards', this.remainList)
        new Notification('私信发送完成', {
          body: '共有 ' + this.remainList.length + ' 条发送失败'
        })
        this.remainList = []
      }
    },
    startRecalling() {
      console.log('start recall')
      this.inProgress = true
      this.recallNext()
    },
    recallNext() {
      console.log('recall')
      if (this.sentMsgList.length > 0) {
        const sentMsg = this.sentMsgList.shift()
        if (new Date().getTime() - sentMsg.timestamp > 120000) {
          // 消息超过两分钟无法撤回 终止循环
          new Notification('私信撤回失败', { body: '无法撤回超过两分钟的消息' })
          this.remainList.push(sentMsg)
          this.remainList.push(...this.sentMsgList)
          this.sentMsgList = []
          setTimeout(this.recallNext, this.setting.sendInterval)
        } else {
          // 调用撤回消息接口
          this.Bilibili.recallMessage(
            sentMsg.uid,
            this.Store.get('loginResponse'),
            sentMsg.msg_key
          )
            .then(() => {
              console.log('recall', 'success', sentMsg)
              setTimeout(this.recallNext, this.setting.sendInterval)
            })
            .catch((e) => {
              this.remainList.push(sentMsg)
              new Notification('私信撤回失败', {
                body:
                  '撤回' + sentMsg.username + '私信时遇到了问题：' + e.message
              })
              if (e.code === 21041) {
                // 如果消息超过两分钟导致失败 终止循环
                this.remainList.push(...this.sentMsgList)
                this.sentMsgList = []
              }
              setTimeout(this.recallNext, this.setting.sendInterval)
            })
        }
      } else {
        // End of message recall
        this.inProgress = false
        new Notification('私信撤回完成', {
          body: '共有 ' + this.remainList.length + ' 条撤回失败'
        })
        this.remainList = []
      }
    }
  }
}
</script>

<style scoped>
.template-textarea :deep(.v-input__control) {
  background-color: rgba(255, 255, 255, 0.05) !important;
}

.template-textarea :deep(.v-text-field__details) {
  background-color: transparent !important;
}

.message-textarea :deep(.v-input__control) {
  background-color: rgba(255, 255, 255, 0.05) !important;
}

.message-textarea :deep(.v-text-field__details) {
  background-color: transparent !important;
}

.template-textarea :deep(.v-input__slot),
.message-textarea :deep(.v-input__slot) {
  border: 1px solid rgba(255, 255, 255, 0.3) !important;
  border-radius: 4px;
}

.template-textarea :deep(textarea),
.message-textarea :deep(textarea) {
  color: rgba(255, 255, 255, 0.87) !important;
  background-color: rgba(255, 255, 255, 0.02) !important;
}
</style>
