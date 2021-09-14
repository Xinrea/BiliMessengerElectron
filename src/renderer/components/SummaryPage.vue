<template>
  <div>
    <setting-alert-page v-if="roomID === ''" />
    <div
      v-else
    >
      <v-row>
        <v-col
          cols="5"
        >
          <v-card>
            <v-card-text>
              <v-hover v-slot="{ hover }">
                <v-img
                  :src="roomInfo['cover']"
                  :class="{ 'rounded-lg': true}"
                >
                  <v-badge
                    v-if="roomInfo.live_status === 1"
                    color="orange"
                    style="position: absolute; top: 25px; left: 10px; opacity: 0.8;"
                    :content="'人气值：'+roomInfo.online"
                  />
                  <div
                    class="d-flex flex-column justify-center align-center"
                    :class="{'on-hover':hover}"
                    style="height: 100%; width: 100%;"
                  >
                    <div class="align-self-center">
                      <v-btn
                        :class="{ 'show-btns': hover }"
                        icon
                        width="80"
                        height="80"
                        @click="open('https://live.bilibili.com/'+roomID)"
                      >
                        <v-icon
                          :class="{ 'show-btns': hover }"
                          size="50"
                          color="rgb(0,0,0,0)"
                        >
                          mdi-play
                        </v-icon>
                      </v-btn>
                    </div>
                  </div>
                </v-img>
              </v-hover>
              <div class="text-overline">
                {{ roomInfo.title }}
              </div>
              <v-divider />
              <div class="mt-3 d-flex align-center">
                <v-avatar
                  color="dark"
                  size="40"
                  class="mr-3"
                >
                  <img
                    :src="userInfo.base_info.face"
                    alt="头像"
                  >
                </v-avatar>
                <span
                  class="pointer"
                  @click="open('https://space.bilibili.com/'+roomInfo.uid)"
                >
                  {{ userInfo.base_info.uname }}
                </span>
                <v-spacer />
                <v-chip
                  class="ma-2"
                  color="orange"
                  text-color="white"
                >
                  关注 {{ userInfo.relation_info.attention }}
                </v-chip>
              </div>
            </v-card-text>
          </v-card>
        </v-col>
        <v-col cols="7">
          <v-card
            elevation="2"
            class="pa-4"
          >
            <line-chart
              :chart-data="guardData"
              :options="guardOption"
            />
          </v-card>
        </v-col>
      </v-row>
      <v-row>
        <v-card
          elevation="2"
          class="ma-3 pa-4"
          width="100%"
        >
          <line-chart
            :chart-data="chartData"
            :options="chartOption"
          />
        </v-card>
        <v-row class="ma-1">
          <v-spacer />
          <div class="text-body-2">
            *图表数据来源自 vtbs.moe
          </div>
        </v-row>
      </v-row>
    </div>
  </div>
</template>

<script>
  import SettingAlertPage from './SettingAlertPage.vue'
  import LineChart from './LineChart'
  import moment from 'moment'

  export default {
    name: 'SummaryPage',
    components: {LineChart, SettingAlertPage },
    data: function () {
      return {
        chartOption: {
          aspectRatio: 3,
          backgroundColor: 'rgba(75,75,75,0.84)',
          color: 'rgba(255,255,255,0.77)'
        },
        guardOption: {
          aspectRatio: 1.91,
          backgroundColor: 'rgba(75,75,75,0.84)',
          color: 'rgba(255,255,255,0.77)'
        },
        chartData: {
          labels: [],
          datasets:[{
            label: '粉丝数',
            data: [],
            borderColor: 'orange'
          }]
        },
        guardData: {
          labels: [],
          datasets:[{
            label: '舰长数',
            data: [],
            borderColor: 'orange'
          }]
        },
        roomInfo: {
          cover: '',
          title: '',
          uid: 61639371,
          online: 0,
          live_status: 2,
          live_start_time: 0
        },
        userInfo: {
          base_info: {
            uname: '',
            face: ''
          },
          relation_info: {
            attention: 0
          }
        },
        roomID: '',
      }
    },
    mounted () {
      this.roomID = this.Store.get('roomID', '21484828')
      if (this.roomID !== '') {
        this.updateInfo()
      }
      this.Store.onDidChange('roomID', ((newValue) => {
        console.log('Room Changed', newValue)
        this.roomID = newValue
        if (newValue !== '') {
          this.updateInfo()
        }
      }))
    },
    methods: {
      open (link) {
        this.$electron.shell.openExternal(link)
      },
      updateInfo() {
        let that = this
        this.Bilibili.getRoomInfo(this.roomID).then(data=>{
          console.log(that.roomID, data)
          that.roomInfo = data['room_info']
          that.userInfo = data['anchor_info']
          let chartData = {
            labels: [],
            datasets:[{
              label: '粉丝数',
              data: [],
              borderColor: 'orange',
              borderWidth: 0.9,
              pointStyle: 'cross',
              color: 'orange'
            }]
          }
          let guardData = {
            labels: [],
            datasets:[{
              label: '舰长数',
              data: [],
              borderColor: 'orange',
              borderWidth: 0.9,
              pointStyle: 'cross',
              color: 'orange'
            }]
          }
          that.Bilibili.getGuardHistory(that.roomInfo.uid).then(data=>{
            data.reverse()
            let beginDay = moment.unix(data[0].time/1000)
            let dayCount = 60
            for(let i = 0; i < data.length; i++) {
              let item = data[i]
              let current = moment.unix(item.time/1000)
              if (current.format('YYYY-MM-DD') === beginDay.format('YYYY-MM-DD') || current.isBefore(beginDay, 'day')) {
                guardData.datasets[0].data.push(item.guardNum)
                guardData.labels.push(current.format('MM-DD'))
                if (current.isBefore(beginDay, 'day')) {
                  beginDay = current
                } else {
                  beginDay.subtract(1,'days')
                }
                dayCount -= 1
                if (dayCount < 0) {
                  break
                }
              }
            }
            guardData.labels.reverse()
            guardData.datasets[0].data.reverse()
            that.guardData = guardData
          })
          that.Bilibili.getFollowerHistory(that.roomInfo.uid).then(data=>{
            data.reverse()
            let beginDay = moment.unix(data[0].time/1000)
            let dayCount = 60
            for(let i = 0; i < data.length; i++) {
              let item = data[i]
              let current = moment.unix(item.time/1000)
              if (current.format('YYYY-MM-DD') === beginDay.format('YYYY-MM-DD') || current.isBefore(beginDay, 'day')) {
                chartData.datasets[0].data.push(item.follower)
                chartData.labels.push(current.format('MM-DD'))
                if (current.isBefore(beginDay, 'day')) {
                  beginDay = current
                } else {
                  beginDay.subtract(1,'days')
                }
                dayCount -= 1
                if (dayCount < 0) {
                  break
                }
              }
            }
            chartData.labels.reverse()
            chartData.datasets[0].data.reverse()
            that.chartData = chartData
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
/* .v-img{
  transition: opacity .4s ease-in-out;
} */
.show-btns {
  color: rgb(255, 255, 255) !important;
}
.on-hover {
  background-color: rgba(0, 0, 0, 0.4);
}
</style>
