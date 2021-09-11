<template>
  <setting-alert-page v-if="!isSet" />
  <div v-else>
    <v-card>
      <v-card-text>
        <v-chip
          class="ma-2"
          color="rgb(213, 166, 112)"
        >
          总数：{{ statistic.total }}
        </v-chip>
        <v-chip
          class="ma-2"
          color="rgb(220, 113, 113)"
        >
          总督：{{ statistic.type1 }}
        </v-chip>
        <v-chip
          class="ma-2"
          color="rgb(190, 119, 198)"
        >
          提督：{{ statistic.type2 }}
        </v-chip>
        <v-chip
          class="ma-2"
          color="rgb(124, 168, 202)"
        >
          舰长：{{ statistic.type3 }}
        </v-chip>
        <v-menu
          v-model="menu"
          :close-on-content-click="false"
          offset-y
          offset-x
        >
          <template #activator="{ on, attrs }">
            <v-btn
              small
              fab
              style="float: right"
              v-bind="attrs"
              v-on="on"
            >
              <v-icon>
                mdi-calendar
              </v-icon>
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
      </v-card-text>
      <v-list
        subheader
      >
        <v-list-item-group
          v-model="selecttedItem"
        >
          <v-list-item
            v-for="(guard) in guards"
            :key="guard.uid"
          >
            <v-list-item-avatar>
              <img
                :src="guard.face"
                alt="avatar"
              >
            </v-list-item-avatar>
            <v-list-item-content
              class="ml-3"
            >
              <v-list-item-title>
                <v-chip
                  class="mr-2"
                  :color="guard.is_alive === 1 ? 'blue' : 'orange'"
                  label
                  outlined
                  small
                >
                  {{ guardLevelToString(guard.guard_level) }}
                </v-chip>
                {{ guard.username }}
              </v-list-item-title>
            </v-list-item-content>
            <v-list-item-action>
              <v-btn
                icon
                @click="open('https://space.bilibili.com/'+guard.uid)"
              >
                <v-icon color="grey lighten-1">
                  mdi-information
                </v-icon>
              </v-btn>
            </v-list-item-action>
            <v-list-item-action>
              <v-btn
                icon
                @click="open('https://message.bilibili.com/#/whisper/mid'+guard.uid)"
              >
                <v-icon color="grey lighten-1">
                  mdi-send
                </v-icon>
              </v-btn>
            </v-list-item-action>
          </v-list-item>
        </v-list-item-group>
      </v-list>
    </v-card>
  </div>
</template>

<script>
import SettingAlertPage from './SettingAlertPage'

export default {
  name: 'GuardListPage',
  components: { SettingAlertPage },
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
      default: '21484828'
    }
  },
  data() {
    return {
      selecttedItem: null,
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
      }
    }
  },
  watch:{
    rid() {
      this.updateList()
    }
  },
  mounted () {
    this.updateList()
  },
  methods: {
    open (link) {
      this.$electron.shell.openExternal(link)
    },
    guardLevelToString (level) {
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
    updateList() {
      let that = this
      let dateNow = this.currentDate()
      this.datePick.allowedList = [dateNow]
      this.Bilibili.getGuardValidDate(this.rid).then(d=>{
        console.log('Update DatePicker')
        that.datePick.allowedList.push(...d)
      })
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
        that.Bilibili.getGuardHistoryList(this.rid, this.datePick.currentDate).then(d=>{
          that.guards = []
          d.forEach(g=>{
            that.guards.push({
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
          this.updateStatistic()
        })
      } else {
        this.updateLastGuardList()
      }
    },
    updateLastGuardList() {
      let that = this
      this.Bilibili.getRoomInfo(this.rid).then(d=>{
        console.log('Get uid from rid', that.rid, d.room_info.uid)
        that.Bilibili.getGuardList(d.room_info.uid, 1, 30).then(r=>{
          console.log('GetGuardList')
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
    }
  }
}
</script>

<style>

</style>