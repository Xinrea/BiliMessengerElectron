<template>
  <div>
    <setting-alert-page v-if="roomID === ''" />
    <div
      v-else
      class="d-flex justify-start"
    >
      <v-card
        width="50%"
      >
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
          <div class="mt-3">
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
            <v-chip
              class="ma-2"
              color="orange"
              text-color="white"
              style="float: right"
            >
              关注 {{ userInfo.relation_info.attention }}
            </v-chip>
          </div>
        </v-card-text>
      </v-card>
      <v-card
        class="ml-3"
        width="50%"
      >
        <v-card-text>
          <v-sheet
            color="rgba(0,0,0,0.3)"
            elevation="2"
            class="rounded-lg pa-2"
          >
            <line-chart
              :chart-data="chartData"
              :options="chartOption"
            />
          </v-sheet>
          <v-row class="ma-1">
            <v-spacer />
            <div class="text-body-2">
              *图表数据来源自 vtbs.moe
            </div>
          </v-row>
        </v-card-text>
      </v-card>
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
          aspectRatio: 2,
          backgroundColor: 'rgba(239,239,235,0.84)',
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
              color: 'white'
            }]
          }
          that.Bilibili.getFollowerHistory(that.roomInfo.uid).then(data=>{
            data.reverse()
            console.log('Get History',that.roomInfo.uid, data)
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
