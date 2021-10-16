<template>
  <v-card
    height="100%"
  >
    <v-toolbar>
      兑换码总数：{{ codes.length }}
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
            导入
          </v-btn>
        </template>
        <v-card>
          <v-card-title>
            添加兑换码
          </v-card-title>
          <v-card-subtitle>
            一行一个，空行会被自动移除；爱发电可批量生成并复制粘贴于此
          </v-card-subtitle>
          <v-card-text>
            <v-textarea
              v-model="codeImport"
            />
            <v-btn
              color="primary"
              @click="importCodes"
            >
              添加
            </v-btn>
          </v-card-text>
        </v-card>
      </v-dialog>
      <v-btn
        small
        color="error"
        class="ml-1"
        @click="deleteCodes"
      >
        <v-icon>
          {{ Mdi.mdiDelete }}
        </v-icon>
        删除
        {{ codeSelected.length }}
      </v-btn>
      <v-btn
        small
        class="ml-1"
        color="#902E2E"
        @click="emptyCodes"
      >
        <v-icon>
          {{ Mdi.mdiDeleteEmpty }}
        </v-icon>
        清空
      </v-btn>
    </v-toolbar>
    <v-list
      subheader
      style="bottom: 0; top: 60px; left: 0; right: 0; position: absolute"
    >
      <v-virtual-scroll
        :bench="5"
        :items="codes"
        class="mb-1"
        item-height="48"
      >
        <template #default="{ item }">
          <v-list-item-group
            v-model="codeSelected"
            multiple
          >
            <v-list-item
              :key="item"
              :value="item"
            >
              <v-list-item-content>
                {{ item }}
              </v-list-item-content>
            </v-list-item>
          </v-list-item-group>
        </template>
      </v-virtual-scroll>
    </v-list>
  </v-card>
</template>

<script>
export default {
  name: 'CodeManagePage',
  data() {
    return {
      codeImport: '',
      codes: ['code1', 'code2'],
      codeSelected: [],
      addDialog: false
    }
  },
  mounted () {
    this.loadCodes()
  },
  methods: {
    loadCodes() {
      this.codes = this.Store.get('codes', [])
      this.Store.onDidChange('codes', v=>{
        this.codes = v
      })
    },
    deleteCodes() {
      console.log('Delete code', this.codeSelected)
      for (let index = this.codes.length-1; index >= 0; index--) {
        if (this.codeSelected.includes(this.codes[index])) {
          this.codes.splice(index,1)
        }
      }
      this.codeSelected = []
      this.Store.set('codes', this.codes)
    },
    importCodes() {
      let newCodes = this.codeImport.split('\n').filter(c=>{
        return c !== ''
      })
      this.codes.push(...newCodes)
      this.codes = Array.from(new Set(this.codes))
      this.addDialog = false
      this.Store.set('codes', this.codes)
    },
    emptyCodes() {
      this.codes = []
      this.codeSelected = []
      this.Store.set('codes', [])
    }
  }
}
</script>

<style scoped>

</style>