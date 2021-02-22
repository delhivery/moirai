import {
  Construct as CdkConstruct,
  Stack as CdkStack,
  StackProps as CdkStackProps,
} from "@aws-cdk/core";

export default class Application extends CdkStack {
  public readonly output: { [key: string]: any };

  constructor(scope: CdkConstruct, id: string, props?: CdkStackProps) {
    super(scope, id, props);
    this.output = {};
  }
}
