import { Construct as CdkConstruct, Stage as CdkStage } from "@aws-cdk/core";
import { StackProps } from "./typedefs";
import Application from "./application";

export interface StageStackProps extends StackProps {
  stacks: (new (
    scope: CdkConstruct,
    id: string,
    props: StackProps
  ) => Application)[];
}
/**
 * A stage to deploy all stacks in the application
 *
 * Defines cloudformation stack for the application and instantiates them in order
 *
 * Automatically manages the following:
 *
 * - Stack initialization order
 * - Property forwarding
 */
export default class Stage extends CdkStage {
  constructor(scope: CdkConstruct, id: string, props: StageStackProps) {
    super(scope, id, props);

    props.stacks.reduce(
      (
        accumulated: StackProps,
        ApplicationStack: new (
          scope: CdkConstruct,
          id: string,
          props: StackProps
        ) => Application
      ): StackProps => {
        const stack = new ApplicationStack(
          this,
          ApplicationStack.prototype.constructor.name,
          props
        );
        return {
          ...accumulated,
          ...stack.output,
        };
      },
      props
    );
  }
}
