import { NgModule } from '@angular/core'
import { RouterModule, Routes } from '@angular/router'
import { InfluxdbComponent } from './components/influxdb/influxdb.component'
import { HomeComponent } from './components/home/home.component'
import { LogsComponent } from './components/logs/logs.component'
import { SettingsComponent } from './components/settings/settings.component'
import { NetworkComponent } from './components/network/network.component'
import { SwarmComponent } from './components/swarm/swarm.component'
import { AppLayoutComponent } from './layout/app.layout.component'

const routes: Routes = [
  {
    path: '',
    component: AppLayoutComponent,
    children: [
      {
        path: '',
        component: HomeComponent
      },
      {
        path: 'logs',
        component: LogsComponent
      },
      {
        path: 'network',
        component: NetworkComponent
      },
      {
        path: 'settings',
        component: SettingsComponent
      },
      {
        path: 'influxdb',
        component: InfluxdbComponent
      },
      {
        path: 'swarm',
        component: SwarmComponent
      }
    ]
  }
]

@NgModule({
  imports: [RouterModule.forRoot(routes)],
  exports: [RouterModule]
})
export class AppRoutingModule {}
