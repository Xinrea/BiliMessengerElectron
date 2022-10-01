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
      该列表内的用户将会收到群发私信<br>
      列表可生成自某日的 <b>直播间舰长列表快照</b> 或生成自某时间段的 <b>上舰记录（未去重）</b> PS.只能查看工具当前登录账号的上舰记录
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
                {{ Mdi.mdiCalendar }}
              </v-icon>
              <span class="ml-1">列表快照</span>
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
        <v-menu
          v-model="range_menu"
          :close-on-content-click="false"
          offset-y
          offset-x
        >
          <template #activator="{ on, attrs }">
            <v-btn
              small
              color="#DE9E08"
              class="ml-2"
              v-bind="attrs"
              v-on="on"
            >
              <v-icon>
                {{ Mdi.mdiCalendar }}
              </v-icon>
              <span class="ml-1">上舰记录</span>
            </v-btn>
          </template>
          <v-date-picker
            v-model="datePick.range"
            color="orange"
            class="mt-4"
            elevation="15"
            locale="zh-cn"
            range
            @change="updateListFromRange"
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
                {{ Mdi.mdiPlusBox }}
              </v-icon>
              单个添加
            </v-btn>
          </template>
          <v-card>
            <v-card-title>
              添加用户
            </v-card-title>
            <v-card-subtitle>
              <br>手动添加额外用户，默认使用UID，可选用用户名添加模式；<br>称呼为“总督/提督/舰长”或自定义
            </v-card-subtitle>
            <v-card-text>
              <v-switch
                v-model="addUser.mode"
                label="用户名添加"
                inset
              />
              <v-text-field
                v-if="!addUser.mode"
                v-model="addUser.keyword"
                label="UID"
              />
              <v-text-field
                v-else
                v-model="addUser.keyword"
                label="用户名"
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
          color="green"
          class="ml-1"
          @click="importFromFile"
        >
          <v-icon>
            {{ Mdi.mdiImport }}
          </v-icon>
          导入列表
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
                {{ Mdi.mdiExport }}
              </v-icon>
              导出列表
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
        <v-btn
          small
          class="ml-1"
          color="error"
          @click="deleteGuards"
        >
          <v-icon>
            {{ Mdi.mdiDelete }}
          </v-icon>
          删除
          {{ selecttedItem.length }}
        </v-btn>
        <v-btn
          small
          class="ml-1"
          color="error"
          @click="emptyGuards"
        >
          <v-icon>
            {{ Mdi.mdiDeleteEmpty }}
          </v-icon>
          清空
        </v-btn>
      </v-row>
    </v-card-text>
    <v-divider />
    <v-list
      subheader
      style="bottom: 0; top: 162px; left: 0; right: 0; position: absolute"
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
                    v-if="item.guard_level !== ''"
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
                <v-list-item-subtitle v-if="item.time">
                  {{ item.time }}
                </v-list-item-subtitle>
              </v-list-item-content>
              <v-list-item-action>
                <v-btn
                  icon
                  @click="open('https://space.bilibili.com/'+item.uid)"
                >
                  <v-icon color="grey lighten-1">
                    {{ Mdi.mdiInformation }}
                  </v-icon>
                </v-btn>
              </v-list-item-action>
              <v-list-item-action>
                <v-btn
                  icon
                  @click="open('https://message.bilibili.com/#/whisper/mid'+item.uid)"
                >
                  <v-icon color="grey lighten-1">
                    {{ Mdi.mdiSend }}
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
      range_menu: false,
      datePick: {
        currentDate: '2021-09-11',
        allowedList:  [],
        range: []
      },
      addDialog: false,
      addUser: {
        mode: false,
        keyword: "",
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
      this.updateList()
      this.updateStatistic()
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
      this.updateStatistic()
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
    updateListFromRange() {
      let that = this
      let loginResponse = this.Store.get('loginResponse', null)
      if (loginResponse === null) {
        return
      }
      let range = this.datePick.range.sort((a,b)=>{
          const da = new Date(a)
          const db = new Date(b)
          return da.getTime() - db.getTime()
        })
      console.log(range)
      this.Bilibili.getReceivedGuardsByPeriod(loginResponse, range[0], range[1]).then((res)=>{
        let guards = []
        for (let g of res) {
          guards.push({
            face: './static/noface.jpg',
            username: g.uname,
            guard_level: g.gift_name,
            is_alive: 0,
            rank: 1,
            ruid: 61639371,
            uid: g.uid,
            medal_info: {
              medal_level: 27,
              medal_name: "轴芯"
            },
            time: g.time
          })
        }
        guards = guards.sort((a,b)=>{
          const da = new Date(a)
          const db = new Date(b)
          return da.getTime() - db.getTime()
        })
        that.guards = guards
        console.log('Update List From Range', that.guards)
        this.updateStatistic()
      }).catch(e=>{
        console.error(e)
        new Notification("上舰记录获取失败", {
          body: e
        })
      })
    },
    addGuard() {
      let that = this
      let loginResponse = this.Store.get('loginResponse', null)
      console.log('Add Guard', this.addUser)
      this.addDialog = false
      if (this.addUser.keyword == '') {
          that.showSnackBar(`请输入UID或用户名`)
          return
      }
      if (this.addUser.mode) {
        // Username
        console.log("Search with username")
        this.Bilibili.getUserInfoBySearch(loginResponse, this.addUser.keyword).then(d=>{
          console.log(d)
          // face_nft: 0
          // face_nft_type: 0
          // fans: 251
          // gender: 1
          // hit_columns: ['uname']
          // is_live: 0
          // is_senior_member: 0
          // is_upuser: 1
          // level: 6
          // mid: 475210
          // official_verify: {type: 127, desc: ''}
          // res: (3) [{…}, {…}, {…}]
          // room_id: 843610
          // type: "bili_user"
          // uname: "Xinrea"
          // upic: "//i2.hdslb.com/bfs/face/36e5fa47a770fef9ad64db390ef8059b5ec0ecab.jpg"
          // usign: "好好放个假"
          // verify_info: ""
          for (let i = 0; i < that.guards.length; i++) {
            if (that.guards[i].uid === d.mid) {
              that.showSnackBar(`用户${that.addUser.keyword}已存在`)
              return
            }
          }
          that.guards.unshift({
            face: "https:"+d.upic,
            username: d.uname,
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
      } else {
        // UID
        for (let i = 0; i < this.guards.length; i++) {
          if (this.guards[i].uid.toString() === that.addUser.keyword) {
            that.showSnackBar(`用户${that.addUser.keyword}已存在`)
            return
          }
        }
        this.Bilibili.getUserInfo(loginResponse, this.addUser.keyword).then(d=>{
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
      }
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
        if (this.guards[i].time) {
          output += `${this.guardLevelToString(this.guards[i].guard_level)} ${this.guards[i].uid} ${this.guards[i].username} ${this.guards[i].time.replace(' ', 'T')}\n`
        }
        else output += `${this.guardLevelToString(this.guards[i].guard_level)} ${this.guards[i].uid} ${this.guards[i].username}\n`
      }
      dialog.showSaveDialog({
        title: '导出为 TXT 文件',
        defaultPath: `${this.roomID}-${this.datePick.currentDate}.txt`,
        filters: [{name: 'txt', extensions: ['txt']}]
      }).then(res=>{
        if (!res.canceled) {
          console.log(res.filePath)
          const fs = require('fs')
          fs.writeFileSync(res.filePath, output, 'utf-8')
        }
      })
    },
    importFromFile() {
      let that = this
      dialog.showOpenDialog({
        title: '选择文件',
        filters: [{name: 'list', extensions: ['txt', 'csv']}]
      }).then(res=>{
        if (!res.canceled) {
          console.log(res.filePaths)
          const fs = require('fs');
          fs.readFile(res.filePaths[0], (err, data)=>{
            if (err != null) {
              console.error(err)
              return
            }
            const content = data.toString().split('\n')
            console.log(content.length)
            // Empty Current List
            this.emptyGuards()
            // Process Each Line
            for (let i = 0; i < content.length; i++) {
              const elements = res.filePaths[0][res.filePaths[0].length-1] == 't' ? content[i].split(' ') : content[i].split(',');
              if (elements.length < 3) {
                // Skip This Line
                continue
              }
              const tag = elements[0][0] === '\ufeff' ? elements[0].substring(1) : elements[0]
              const uid = elements[1]
              const name = elements[2]
              let tag_type = 4
              switch (tag) {
                case '舰长':
                  tag_type = 3
                  break
                case '提督':
                  tag_type = 2
                  break
                case '总督':
                  tag_type = 1
                  break
                default:
                  tag_type = tag
                  break;
              }
              let ng = {
                face: 'static/noface.jpg',
                username: name,
                guard_level: tag_type,
                is_alive: 0,
                rank: 1,
                ruid: 61639371,
                uid: uid,
                medal_info: {
                  medal_level: 27,
                  medal_name: "轴芯"
                }
              }
              if (elements.length > 3) {
                ng.time = elements[3].replace('T', ' ')
              }
              that.guards.push(ng)
            }
            that.updateStatistic()
          })
        }
      })
    },
    exportCSV() {
      console.log('Export CSV file')
      let output = ``
      for (let i = 0; i < this.guards.length; i++) {
        if (this.guards[i].time) {
          output += `${this.guardLevelToString(this.guards[i].guard_level)},${this.guards[i].uid},${this.guards[i].username},${this.guards[i].time.replace(' ', 'T')}\n`
        } else {
          output += `${this.guardLevelToString(this.guards[i].guard_level)},${this.guards[i].uid},${this.guards[i].username}\n`
        }
      }
      dialog.showSaveDialog({
        title: '导出为 CSV 文件',
        defaultPath: `${this.roomID}-${this.datePick.currentDate}.csv`,
        filters: [{name: 'csv', extensions: ['csv']}]
      }).then(res=>{
        if (!res.canceled) {
          console.log(res.filePath)
          const fs = require('fs');
          fs.writeFileSync(res.filePath, `\ufeff${output}`, 'utf-8')
        }
      })
    }
  }
}
</script>

<style>

</style>