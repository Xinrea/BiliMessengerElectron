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
              :src="roominfo['cover']"
              :class="{ 'rounded-lg': true}"
            >
              <v-badge
                v-if="roominfo.live_status == 1"
                color="orange"
                style="position: absolute; top: 25px; left: 10px; opacity: 0.8;"
                :content="'人气值：'+roominfo.online"
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
            {{ roominfo.title }}
          </div>
          <v-divider />
          <div class="mt-3">
            <v-avatar
              color="dark"
              size="40"
              class="mr-3"
            >
              <img :src="userinfo.base_info.face">
            </v-avatar>
            {{ userinfo.base_info.uname }}
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
              :labels="graph.label"
              :value="graph.value"
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
          value: [
            4,
            2,
            3,
            10,
            80,
            50,
          ],
          label: [
            '5月',
            '6月',
            '7月',
            '8月',
            '9月',
            '10月',
          ]
        },
        roominfo: {
          cover: '',
          title: ''
        },
        userinfo: {
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
      this.updateInfo()
    },
    methods: {
      open (link) {
        this.$electron.shell.openExternal(link)
      },
      updateInfo() {
        let that = this
        this.Bilibili.getRoomInfo(this.rid, res=>{
          console.log(res)
          that.roominfo = res['room_info']
          that.userinfo = res['anchor_info']
        }, ()=>{

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
  background-color: rgb(0, 0, 0,0.4);
}
</style>
