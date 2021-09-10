<template>
  <div>
    <setting-alert-page v-if="!isSet" />
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
                    @click="open('https://live.bilibili.com/'+rid)"
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
            >
              关注数：{{ userInfo.relation_info.attention }}
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
            color="rgba(0,0,0,.15)"
            class="rounded-lg"
          >
            <v-sparkline
              :labels="graph.timestamp"
              :value="graph.follower"
              stroke-linecap="round"
              line-width="1.2"
              color="orange"
              smooth
            />
          </v-sheet>
        </v-card-text>
      </v-card>
    </div>
  </div>
</template>

<script>
  import SettingAlertPage from './SettingAlertPage.vue'
  export default {
    name: 'SummaryPage',
    components: { SettingAlertPage },
    props: {
      isSet:{
        type: Boolean,
        default: false
      },
      userdata:{
        type: Object,
        default: null
      },
      rid: {
        type: String,
        default: '21484828'
      }
    },
    data: function () {
      return {
        graph: {
          follower: [1],
          timestamp: [100]
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
        }
      }
    },
    watch: {
      rid() {
        this.updateInfo()
      }
    },
    mounted () {
    },
    methods: {
      open (link) {
        this.$electron.shell.openExternal(link)
      },
      updateInfo() {
        let that = this
        this.Bilibili.getRoomInfo(this.rid).then(data=>{
          console.log(that.rid, data)
          that.roomInfo = data['room_info']
          that.userInfo = data['anchor_info']
          that.Bilibili.getFollowerHistory(that.roomInfo.uid).then(data=>{
            console.log(that.roomInfo.uid, data)
            that.graph = {
              follower: [],
              timestamp: []
            }
            data.forEach(item=>{
              that.graph.follower.push(item.follower)
              that.graph.timestamp.push(item.time)
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
