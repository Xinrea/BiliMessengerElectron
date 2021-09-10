<template>
  <setting-alert-page v-if="!isSet" />
  <div v-else>
    list
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
    }
  }
}
</script>

<style>

</style>