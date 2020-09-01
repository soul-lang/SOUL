import EventEmitter from 'events';
import {
  EventBridge,
} from 'juce-blueprint';


class ParameterChangeNotifier extends EventEmitter {
  constructor() {
    super();

    this.CHANGE_EVENT = 'change_';

    this.setMaxListeners(200);
    this._onParameterValueChange = this._onParameterValueChange.bind(this);

    EventBridge.addListener('parameterValueChange', this._onParameterValueChange);

    this.state = {};
  }

  _onParameterValueChange(paramId) {
    this.emit(this.CHANGE_EVENT + paramId, paramId);
  }

  addParameterListener(paramID, listener) {
    this.addListener(this.CHANGE_EVENT + paramID, listener);
  }

  removeParameterListener(paramID, listener) {
    this.removeListener(this.CHANGE_EVENT + paramID, listener);
  }
}

const __singletonInstance = new ParameterChangeNotifier();

export default __singletonInstance;
