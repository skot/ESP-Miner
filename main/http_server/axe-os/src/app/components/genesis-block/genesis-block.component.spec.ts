import { ComponentFixture, TestBed } from '@angular/core/testing';

import { GenesisBlockComponent } from './genesis-block.component';

describe('GenesisBlockComponent', () => {
  let component: GenesisBlockComponent;
  let fixture: ComponentFixture<GenesisBlockComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [GenesisBlockComponent]
    })
    .compileComponents();
    
    fixture = TestBed.createComponent(GenesisBlockComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
