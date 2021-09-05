<template>
  <div>
    <setting-alert-page v-if="!isSet" />
    <div v-else>
      <v-card>
        <v-card-text>
          <div class="d-flex">
            <v-img
              :src="cover"
              max-width="70%"
            />
            <div>
              Others
            </div>
          </div>
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
        cover: ""
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

</style>
