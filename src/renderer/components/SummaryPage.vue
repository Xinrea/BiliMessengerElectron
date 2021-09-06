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
              :src="cover"
              :class="{ 'rounded-lg': true}"
            >
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
        cover: "",
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
          that.cover = res['room_info']['cover']
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
