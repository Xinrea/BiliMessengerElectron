<template>
  <setting-alert-page v-if="roomID === ''" />
  <v-card
    v-else
    height="100%"
  >
    <v-card-title>
      编辑列表
      <v-spacer />
      <v-chip
        class="ma-1"
        color="rgb(213, 166, 112)"
      >
        总数：{{ statistic.total }}
      </v-chip>
      <v-chip
        class="ma-1"
        color="rgb(220, 113, 113)"
      >
        总督：{{ statistic.type1 }}
      </v-chip>
      <v-chip
        class="ma-1"
        color="rgb(190, 119, 198)"
      >
        提督：{{ statistic.type2 }}
      </v-chip>
      <v-chip
        class="ma-1"
        color="rgb(124, 168, 202)"
      >
        舰长：{{ statistic.type3 }}
      </v-chip>
    </v-card-title>
    <v-card-subtitle>
      该列表内的用户将会收到群发私信
    </v-card-subtitle>
    <v-card-text>
      <v-row class="ma-0">
        <v-menu
          v-model="menu"
          :close-on-content-click="false"
          offset-y
          offset-x
        >
          <template #activator="{ on, attrs }">
            <v-btn
              small
              color="#DE9E08"
              v-bind="attrs"
              v-on="on"
            >
              <v-icon>
                mdi-calendar
              </v-icon>
              <span class="ml-1">历史数据</span>
            </v-btn>
          </template>
          <v-date-picker
            v-model="datePick.currentDate"
            :allowed-dates="dateAllowed"
            color="orange"
            class="mt-4"
            elevation="15"
            locale="zh-cn"
            @change="updateGuardHistoryList"
          />
        </v-menu>
        <v-spacer />
        <v-dialog
          v-model="addDialog"
          max-width="600"
        >
          <template #activator="{ on, attrs }">
            <v-btn
              small
              color="green"
              v-bind="attrs"
              v-on="on"
            >
              <v-icon>
                mdi-plus-box
              </v-icon>
              手动添加
            </v-btn>
          </template>
          <v-card>
            <v-card-title>
              添加用户
            </v-card-title>
            <v-card-subtitle>
              手动添加额外用户，称呼为“总督/提督/舰长”或自定义
            </v-card-subtitle>
            <v-card-text>
              <v-text-field
                v-model="addUser.uid"
                label="UID"
              />
              <v-text-field
                v-model="addUser.title"
                label="称呼"
              />
              <v-btn
                color="primary"
                @click="addGuard"
              >
                添加
              </v-btn>
            </v-card-text>
          </v-card>
        </v-dialog>
        <v-btn
          small
          class="ml-1"
          color="error"
          @click="deleteGuards"
        >
          <v-icon>
            mdi-delete
          </v-icon>
          删除
          {{ selecttedItem.length }}
        </v-btn>
        <v-btn
          small
          class="ml-1"
          color="#902E2E"
          @click="emptyGuards"
        >
          <v-icon>
            mdi-delete-empty
          </v-icon>
          清空
        </v-btn>
        <v-menu offset-y>
          <template #activator="{ on, attrs }">
            <v-btn
              small
              class="ml-1"
              color="#4FB02B"
              v-bind="attrs"
              v-on="on"
            >
              <v-icon>
                mdi-export
              </v-icon>
              导出
            </v-btn>
          </template>
          <v-list>
            <v-list-item
              link
              @click="exportTXT"
            >
              <v-list-item-title>导出为 txt 文件</v-list-item-title>
            </v-list-item>
            <v-list-item
              link 
              @click="exportCSV"
            >
              <v-list-item-title>导出为 csv 文件</v-list-item-title>
            </v-list-item>
          </v-list>
        </v-menu>
      </v-row>
    </v-card-text>
    <v-divider />
    <v-list
      subheader
      style="bottom: 0; top: 140px; left: 0; right: 0; position: absolute"
    >
      <v-virtual-scroll
        :bench="5"
        :items="guards"
        class="mb-1"
        item-height="64"
      >
        <template #default="{ item }">
          <v-list-item-group
            v-model="selecttedItem"
            multiple
          >
            <v-list-item
              :key="item.uid"
              :value="item.uid"
            >
              <v-list-item-avatar>
                <img
                  :src="item.face"
                  alt="avatar"
                >
              </v-list-item-avatar>
              <v-list-item-content
                class="ml-3"
              >
                <v-list-item-title>
                  <v-chip
                    class="mr-2"
                    :color="item.is_alive === 1 ? 'blue' : 'orange'"
                    label
                    outlined
                    small
                  >
                    {{ guardLevelToString(item.guard_level) }}
                  </v-chip>
                  {{ item.username }}
                </v-list-item-title>
              </v-list-item-content>
              <v-list-item-action>
                <v-btn
                  icon
                  @click="open('https://space.bilibili.com/'+item.uid)"
                >
                  <v-icon color="grey lighten-1">
                    mdi-information
                  </v-icon>
                </v-btn>
              </v-list-item-action>
              <v-list-item-action>
                <v-btn
                  icon
                  @click="open('https://message.bilibili.com/#/whisper/mid'+item.uid)"
                >
                  <v-icon color="grey lighten-1">
                    mdi-send
                  </v-icon>
                </v-btn>
              </v-list-item-action>
            </v-list-item>
          </v-list-item-group>
        </template>
      </v-virtual-scroll>
    </v-list>
    <v-snackbar
      v-model="snackBar.m"
      timeout="5000"
      right
    >
      {{ snackBar.t }}
      <template #action="{ attrs }">
        <v-btn
          color="red"
          text
          v-bind="attrs"
          @click="snackBar.m = false"
        >
          关闭
        </v-btn>
      </template>
    </v-snackbar>
  </v-card>
</template>

<script>
import SettingAlertPage from './SettingAlertPage'
import {dialog} from '@electron/remote'

export default {
  name: 'GuardListPage',
  components: { SettingAlertPage },
  data() {
    return {
      snackBar: {
        m: false,
        t: ''
      },
      selecttedItem: [],
      guards: [{
        face: './static/noface.jpg',
        username: '加载中',
        guard_level: 1,
        is_alive: 0,
        rank: 1,
        ruid: 61639371,
        uid: 475210,
        medal_info: {
          medal_level: 27,
          medal_name: "轴芯"
        }
      },],
      statistic: {
        total: 0,
        type1: 0,
        type2: 0,
        type3: 0
      },
      menu: false,
      datePick: {
        currentDate: '2021-09-11',
        allowedList:  []
      },
      addDialog: false,
      addUser: {
        uid: "",
        title: ""
      },
      roomID: ''
    }
  },
  mounted () {
    this.roomID = this.Store.get('roomID', '21484828')
    this.guards = this.Store.get('guards', [])
    this.Store.onDidChange('guards', newValue => {
      this.guards = newValue
    })
    this.Store.onDidChange('roomID', newValue => {
      this.roomID = newValue
    })
    this.selecttedItem = []
    this.datePick.currentDate = this.currentDate()
    this.updateCalendar()
    this.updateStatistic()
  },
  methods: {
    open (link) {
      this.$electron.shell.openExternal(link)
    },
    guardLevelToString (level) {
      if (level === level +'') {
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
    updateCalendar() {
      let that = this
      let dateNow = this.currentDate()
      this.datePick.allowedList = [dateNow]
      this.Bilibili.getGuardValidDate(this.roomID).then(d=>{
        console.log('Update DatePicker')
        that.datePick.allowedList.push(...d)
      })
    },
    updateList() {
      this.updateCalendar()
      this.updateLastGuardList()
    },
    updateStatistic() {
      let that = this
      that.statistic = {
        total: 0,
        type1: 0,
        type2: 0,
        type3: 0
      }
      that.guards.forEach(g=>{
        that.statistic.total++
        switch (g.guard_level) {
          case 1:{
            that.statistic.type1++
            break
          }
          case 2:{
            that.statistic.type2++
            break
          }
          case 3:{
            that.statistic.type3++
          }
        }
      })
      this.Store.set('guards', this.guards)
    },
    dateAllowed(v) {
      return this.datePick.allowedList.includes(v)
    },
    currentDate() {
      let date = new Date();
      let mon = date.getMonth() + 1;
      let day = date.getDate();
      return date.getFullYear() + "-"+ (mon<10?"0"+mon:mon) + "-"+(day<10?"0"+day:day);
    },
    updateGuardHistoryList(date) {
      console.log('Update History List',date)
      let that = this
      let dateNow = this.currentDate()
      if (date !== dateNow) {
        // Get History Data From Server
        that.Bilibili.getGuardHistoryList(this.roomID, this.datePick.currentDate).then(d=>{
          that.guards = []
          let newGuards = []
          d.forEach(g=>{
            newGuards.push({
              face: g.Face,
              username: g.Username,
              guard_level: parseInt(g.Guard_level),
              is_alive: 0,
              rank: 1,
              ruid: 61639371,
              uid: g.Uid,
              medal_info: {
                medal_level: 27,
                medal_name: "轴芯"
              }
            })
          })
          that.guards = newGuards
          this.updateStatistic()
        })
      } else {
        this.updateLastGuardList()
      }
    },
    updateLastGuardList() {
      let that = this
      this.Bilibili.getRoomInfo(this.roomID).then(d=>{
        console.log('Get uid from rid', that.roomID, d.room_info.uid)
        that.Bilibili.getGuardList(d.room_info.uid, 1, 30).then(r=>{
          console.log('GetGuardList')
          that.guards = []
          that.guards = r.top3
          that.guards.push(...r.list)
          // Then Get Rest Guards
          let now = r.info.now
          let pages = r.info.page
          let todo = []
          for (let i = now+1; i <= pages; i++) {
            todo.push(that.Bilibili.getGuardList(d.room_info.uid, i, 30))
          }
          Promise.all(todo).then(rs=>{
            rs.forEach(res=>{
              that.guards.push(...res.list)
            })
            this.updateStatistic()
          }).catch(e=>{
            console.error(e)
          })
        }).catch(e=>{
          console.error(e)
        })
      }).catch(e=>{
        console.error(e)
      })
    },
    addGuard() {
      let that = this
      console.log('Add Guard', this.addUser)
      this.addDialog = false
      for (let i = 0; i < this.guards.length; i++) {
        if (this.guards[i].uid.toString() === that.addUser.uid) {
          that.showSnackBar('手动添加的用户已存在')
          return
        }
      }
      this.Bilibili.getUserInfo(this.addUser.uid).then(d=>{
        console.log(d)
        that.guards.unshift({
          face: d.face,
          username: d.name,
          guard_level: that.addUser.title,
          is_alive: 0,
          rank: 1,
          ruid: 61639371,
          uid: d.mid,
          medal_info: {
            medal_level: 27,
            medal_name: "轴芯"
          }
        })
        that.updateStatistic()
      })
    },
    deleteGuards() {
      console.log('Delete Guard', this.selecttedItem)
      let newGuards = this.guards.filter((v)=>{
        return !this.selecttedItem.includes(v.uid)
      })
      this.guards = newGuards
      this.selecttedItem = []
      this.updateStatistic()
    },
    emptyGuards() {
      this.guards = []
      this.selecttedItem = []
      this.updateStatistic()
    },
    showSnackBar(text) {
      this.snackBar.t = text
      this.snackBar.m = true
    },
    exportTXT() {
      console.log('Export TXT file')
      let output = `舰队总数：${this.guards.length}\n`
      for (let i = 0; i < this.guards.length; i++) {
        output += `${this.guardLevelToString(this.guards[i].guard_level)} ${this.guards[i].uid} ${this.guards[i].username}\n`
      }
      let file = dialog.showSaveDialogSync({
        title: '导出为 TXT 文件',
        defaultPath: `${this.roomID}-${this.datePick.currentDate}.txt`,
        filters: [{name: 'txt', extensions: ['txt']}]
      })
      if (file !== undefined) {
        console.log(file)
        const fs = require('fs');
        fs.writeFileSync(file, output, 'ANSI')
      }
    },
    exportCSV() {
      console.log('Export CSV file')
      let output = ``
      for (let i = 0; i < this.guards.length; i++) {
        output += `${this.guardLevelToString(this.guards[i].guard_level)},${this.guards[i].uid},${this.guards[i].username}\n`
      }
      let file = dialog.showSaveDialogSync({
        title: '导出为 CSV 文件',
        defaultPath: `${this.roomID}-${this.datePick.currentDate}.csv`,
        filters: [{name: 'csv', extensions: ['csv']}]
      })
      if (file !== undefined) {
        console.log(file)
        const fs = require('fs');
        fs.writeFileSync(file, output, 'ANSI')
      }
    }
  }
}
</script>

<style>

</style>